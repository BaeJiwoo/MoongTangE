# MoongTangE

매칭 계층은 컨트롤러 기반 ASP.NET Core MVC API와 C++ 게임 소켓 서버를 서로 다른 프로세스·호스트로 배포할 수 있게 분리되어 있습니다. [API 계약과 실행 방법](docs/MATCHMAKING_API.md), [연동 예제](Tools/matchmaking_client.py)를 참고하세요. C++ 서버는 HTTP를 수신하지 않고 외부 API로 등록·heartbeat를 보내며, 최종 예약과 일회용 입장권 소비는 서버 Tick이 처리합니다.

MoongTangE는 C++20 기반의 Windows/Linux 3D 물리 시뮬레이션 서비스입니다. macOS는 지원 대상에서 제외합니다.

C++ 코드는 `MoongTangE` 네임스페이스를 사용하며, Protobuf 패키지는 `MoongTangE.proto`입니다. 빌드 옵션·매크로·환경변수도 `MOONGTANGE_` 접두사로 통일합니다.

## 목표

- Windows와 Linux에서 빌드 가능한 CMake 모노레포
- `Common`: 외부 라이브러리에 의존하지 않는 수학, 자체 물리와 게임 자료형
- `Server`: 싱글플레이와 멀티플레이가 공유하는 서버 코어 및 독립 실행 서버
- `Client`: raylib 3D 렌더링, WASD 입력, 로컬 및 TCP 전송
- 서버 권위 방식의 60Hz 물리 시뮬레이션과 20Hz 상태 전송

## 개발 상태

싱글플레이, TCP 멀티플레이, 자체 구름 물리와 자동 테스트가 구현되어 있습니다. 기본 브랜치는 `main`이며, 전체 구현과 C++ 설계 정리는 이슈별 PR을 순서대로 리뷰하고 병합하여 통합합니다.

인증, 게임 룰, 경기 결과와 재화 관리의 후속 작업 범위와 구현 순서는 [TODO list](TODO.md)에 정리되어 있습니다.

모든 개발 변경은 실제 GitHub 이슈 번호를 사용하는 작업 브랜치에서 진행합니다. 의존성이 있는 변경은 Stacked PR로 제출합니다. 사용자에게 명시적인 병합 요청을 받은 경우에만 코드 리뷰와 검증 후 병합하며, 그 외에는 PR을 열린 상태로 유지합니다.

## 기술 스택

| 영역 | 기술 |
| --- | --- |
| 언어 | C++20 |
| 매칭 API | ASP.NET Core 8 MVC (컨트롤러 기반 Web API) |
| 빌드 | CMake |
| 네트워크 | TCP, Windows IOCP / Linux epoll 네이티브 구현 |
| 직렬화 | Protobuf 36.2 (C++ 런타임 7.36.2), 프로토콜 v8 |
| 렌더링 | raylib 6.0 |
| 테스트 | CTest |
| CI | GitHub Actions |

## 빌드

CMake 3.25 이상, C++20 컴파일러와 Git이 필요합니다. 분리형 매칭 API와 통합 테스트에는 .NET 8 SDK와 Python 3도 필요합니다. 첫 구성 시 고정된 외부 소스를 다운로드하므로 인터넷 연결이 필요합니다. 다운로드는 SHA-256으로 검증합니다.

기존 빌드 폴더도 아래의 현재 옵션 이름으로 다시 구성하세요. 전체 빌드는 `MOONGTANGE_BUILD_CLIENT=ON`, `MOONGTANGE_BUILD_NETWORK=ON`이 기본값이고, 코어 전용 구성은 두 옵션을 명시적으로 `OFF`로 지정합니다. 네트워크 구현은 `MOONGTANGE_NETWORK_BACKEND=AUTO|IOCP|EPOLL`로 선택합니다. `AUTO`는 Windows에서 직접 구현한 IOCP, Linux에서 직접 구현한 epoll을 선택합니다. Windows의 `EPOLL`, Linux의 `IOCP`, 모든 macOS 구성은 CMake 단계에서 거부합니다.

`main`의 서버·클라이언트·테스트는 네이티브 TCP 구현을 사용하며 Asio 소스나 의존성을 포함하지 않습니다. Asio 제거는 [Issue #67](https://github.com/BaeJiwoo/MoongTangE/issues/67)과 [PR #69](https://github.com/BaeJiwoo/MoongTangE/pull/69)에 기록되어 있습니다. 이전 Standalone Asio 구현은 Git 이력과 별도 보존 브랜치에 남아 있으며 `main`의 선택 가능한 백엔드에는 포함하지 않습니다.

Asio 파일이 보이면 `git branch --show-current`로 현재 브랜치를 확인하세요. 이전 브랜치의 소스나 기존 `build/_deps/asio-*` 다운로드 캐시는 현재 `main`의 빌드 의존성을 뜻하지 않습니다. 최신 `main` 기반 소스를 새 빌드 폴더에 구성하면 과거 캐시와 구분할 수 있습니다. 예: `cmake -S . -B build-native -DMOONGTANGE_NETWORK_BACKEND=AUTO`. 이 구성은 Asio를 다운로드하거나 연결하지 않습니다.

네트워크 구성에서는 고정된 Protobuf와 Abseil 소스, 동일 릴리스의 `protoc`도 빌드합니다. 첫 전체 빌드는 이 의존성 컴파일 때문에 더 오래 걸립니다. 별도 Protobuf 설치는 필요하지 않으며 `.proto`로 생성한 파일은 빌드 폴더에만 저장됩니다.

저장소를 받은 뒤 운영체제에 맞는 Release 프리셋을 선택합니다. 프리셋은 빌드와 테스트까지 같은 설정을 사용하며 지원하지 않는 운영체제의 프리셋은 목록에서 숨깁니다.

| 운영체제 | 네트워크 구현 | 프리셋 | 빌드 폴더 |
| --- | --- | --- | --- |
| Windows | 네이티브 IOCP | `windows-release` | `build/windows-release` |
| Linux | 네이티브 epoll | `linux-release` | `build/linux-release` |

```sh
git clone https://github.com/BaeJiwoo/MoongTangE.git
cd MoongTangE
cmake --list-presets
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
```

위 예시의 `linux-release`를 원하는 운영체제·백엔드 프리셋 이름으로 바꾸세요. 사용자 지정 빌드 폴더가 필요하면 기존 `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` 방식도 유지되며, 이때 백엔드는 기본 `AUTO`가 선택합니다. Windows에서 `EPOLL`, Linux에서 `IOCP`를 고르면 구성 단계에서 거부됩니다.

Windows에서는 Visual Studio의 Desktop development with C++ 워크로드를 설치하세요. Linux의 그래픽 빌드 의존성:

```sh
sudo apt-get install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
```

서버만 빌드하려면 `-DMOONGTANGE_BUILD_CLIENT=OFF`를 추가하세요. 외부 라이브러리 없이 코어와 테스트만 빌드하려면 아래 구성을 사용합니다.

```sh
cmake -S . -B build-core -DMOONGTANGE_BUILD_CLIENT=OFF -DMOONGTANGE_BUILD_NETWORK=OFF
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

## VS Code에서 정의로 이동

저장소 폴더를 열면 CMake Tools가 C/C++ 확장에 프로젝트와 외부 라이브러리의 헤더 경로를 전달합니다. 첫 프리셋 선택이 나오면 **MoongTangE**를 선택하세요. 함수·클래스 이름을 **우클릭 → 정의로 이동(Go to Definition)** 하면 해당 코드 파일이 열립니다. 단축키는 `F12`이며, 사용처는 `Shift+F12`, 헤더/구현 전환은 `Alt+O`입니다.

`C/C++`, `CMake Tools`를 추천 확장으로 등록했습니다. 새로 받은 저장소에서는 기존 빌드 절차를 한 번 실행해야 생성 헤더 `game.pb.h`까지 탐색할 수 있습니다.

## 실행

Windows / Visual Studio 빌드:

```powershell
# 싱글플레이: 클라이언트 내부 서버 사용
.\build\windows-release\Release\Client.exe --single

# 멀티플레이: 첫 번째 터미널에서 서버 실행
.\build\windows-release\Release\DedicatedServer.exe --port 27015

# 별도 터미널에서 클라이언트 실행. 여러 개를 실행할 수 있습니다.
.\build\windows-release\Release\Client.exe --connect 127.0.0.1:27015
```

Linux 프리셋의 실행 파일은 `build/linux-release`에 있습니다.

```sh
./build/linux-release/Client --single
./build/linux-release/Client --single --world-seed 42
./build/linux-release/DedicatedServer --port 27015
./build/linux-release/DedicatedServer --port 27015 --world-seed 42
./build/linux-release/Client --connect 127.0.0.1:27015
```

클라이언트는 메인 화면으로 열립니다. `SINGLE PLAYER`를 선택하면 내장 서버를 시작하고,
`MULTIPLAYER`를 선택하면 `--connect` 또는 매치메이킹으로 배정받은 TCP 서버에 접속합니다.
서버 배정 정보가 없으면 멀티플레이 버튼은 비활성화됩니다. `--smoke` 실행은 자동 테스트를
위해 메인 화면을 건너뜁니다.

메인 화면의 `OPTIONS`에서는 출력 모니터와 `WINDOWED`, `FULLSCREEN WINDOWED`(테두리 없는
전체화면), `FULLSCREEN` 모드를 선택할 수 있습니다. 기본 창 크기는 `1280×800`이고 창 모드
해상도는 `1100×720`, `1280×720`, `1280×800`, `1600×900`, `1920×1080`,
`2560×1440`, `3840×2160`(4K)을 제공하며
전체화면 계열은 선택한 모니터의 기본 해상도와 주사율을 사용합니다. raylib는 모니터가
지원하는 전체 영상 모드 목록을 제공하지 않으므로 주사율은 현재 값을 표시하되 임의의
선택지는 만들지 않습니다. VSync와 30, 60, 120, 144, 240 FPS 또는 무제한 프레임 제한도
설정할 수 있습니다. 메뉴, 설정 창, 확인 창, 게임 HUD와 플레이어 이름표는 1100×720 기준
UI 캔버스에서 화면 비율을 유지하며 현재 해상도에 맞게 함께 확대·축소됩니다.

`APPLY`는 변경을 즉시 시험하고 15초 확인 창을 엽니다. `KEEP` 또는 Enter를 누른 설정만
저장하며, `REVERT`, Escape 또는 제한 시간 만료 시 직전에 확인한 설정으로 복구합니다.
일반 설정 화면의 `CANCEL`도 아직 적용하지 않은 선택을 버립니다. 확인한 설정은 Windows의
`%APPDATA%\MoongTangE\display.cfg`, Linux의
`$XDG_CONFIG_HOME/MoongTangE/display.cfg`(미지정 시 `~/.config`)에 저장되어 다음 실행에
적용됩니다.

메인 화면에서 1~16자의 영문·숫자·공백·`_`·`-` 닉네임을 입력합니다. 서버는 멀티플레이
입장 전에 닉네임을 검증하며 거부 사유는 클라이언트 팝업으로 표시됩니다. 입장한 플레이어의
닉네임은 서버 Snapshot으로 전달되어 각 공 위에 작은 이름표로 표시됩니다.

W/A/S/D를 누르면 바닥의 공에 힘이 가해져 서서히 가속합니다. 키를 놓으면 관성으로 굴러가다가 멈추고, 반대 방향 키는 먼저 제동한 뒤 방향을 바꿉니다. 공중에서는 조작력이 작용하지 않습니다. Escape로 종료하며 F3는 복합 구 콜라이더, 활성 접촉점과 접촉 법선을 표시합니다. 자신의 공은 주황색, 다른 플레이어는 파란색이며 흰 무늬가 실제 자세에 맞춰 회전합니다. 중력은 (0,-9.81,0), 기본 질량은 1 kg, 반지름은 0.5 m, 최초 위치는 (0,2,0)입니다. 추가 플레이어는 X축으로 간격을 두고 생성됩니다. 대각선 입력의 힘은 정규화됩니다.

각 방은 seed로 재현 가능한 수집 물체 8개로 시작하고, 참가자가 있는 동안 5초마다 물체 하나를
Y=8에서 떨어뜨립니다. 물체는 중력과 plane 접촉을 적용받으며 최대 24개입니다. 종류는 사과,
바나나, 버거, 도넛이고 각 물체 크기는 0.65–1.25배 범위에서 겹치지 않게 순회합니다.
공과 물체의 종류별 구 프록시 표면이 닿으면 획득하며, 물체 반지름의 25%가 묻히는 위치와
접촉 법선 자세로 9 Tick 동안 감쇠 정착합니다. 기본 공과 모든 부착물의 구 합집합에서 가장
깊은 최대 4개 지형 접촉을 풀고, 부착물 질량과 평행축 관성을 플레이어에 합산합니다. 부착
처리는 순간 충격을 더하지 않으며, 플레이어가 나가면 물체는 다시 자유 낙하합니다.
`--world-seed`로 싱글플레이 또는 독립 서버 배치를 재현할 수 있습니다. 구현 규칙과 CC0 에셋
정보는 [수집 물체 시스템 문서](docs/COLLECTIBLE_SYSTEM.md)에 있습니다.

기본 공은 속이 찬 구의 회전 관성 `I=2mr²/5`를 사용하고, 부착물에는 자체 관성과 `m|c|²` 평행축 항을 더합니다. 접촉점의 미끄럼 속도로 마찰 충격량을 계산해 이동 속도와 각속도를 연결합니다. 바닥 마찰 계수 0.8, 구름 저항 계수 0.12, 바닥 선형 감쇠 1.8 kg/s, 기본 조작력 10 N을 사용합니다. 부착 질량에 따라 조작력을 제한적으로 보정하며, 아주 느려진 공은 정지 처리합니다. 공중에서는 회전 관성이 유지됩니다.

서버 기본 바인딩은 `127.0.0.1`입니다. 신뢰할 수 있는 테스트 LAN에서만 `--public`을 사용해 `0.0.0.0`으로 바인딩하세요. 이 프로토콜에는 사용자 인증이나 암호화가 없으므로 인터넷에 공개하지 마세요. 방화벽 포트 개방은 자동으로 수행하지 않습니다.

`DedicatedServer --run-for 5`는 5초 후 정상 종료합니다. 포트 0은 운영체제가 사용 가능한 테스트 포트를 선택하며 실제 주소가 출력됩니다. `Client --single --smoke` 또는 `Client --connect host:port --smoke`는 숨겨진 그래픽 창에서 서버 이동을 확인한 뒤 자동 종료합니다. `--smoke`도 OpenGL 환경이 필요합니다. 환경변수 `MOONGTANGE_SMOKE_CAPTURE`를 지정하면 종료 직전 화면을 해당 PNG 경로에 저장합니다.

## 3인 게임방 배정

한 서버 안에 독립된 게임월드가 여러 개 실행됩니다. 접속자는 생성 순서가 빠른 방의 빈자리부터 들어가며, 모두 찼을 때 다음 방을 만듭니다. 7명이 차례대로 접속하면 3명 / 3명 / 1명으로 배정됩니다. 앞선 방에 빈자리가 생기면 다음 접속자가 그 자리를 채웁니다. 기존 참가자를 이동시키지는 않습니다.

방은 미리 만들지 않습니다. 기본 최대 8개 방이며 `--max-rooms 2`는 최대 6명으로 제한합니다. 모든 방이 차면 입장을 거절합니다. 빈 방은 기본 30초 후 정리하며 `--empty-room-seconds 0`은 즉시 정리합니다. 각 방은 자기 참가자의 입력과 상태만 처리합니다. 이동 입력은 유저당 최신 값 하나를 보관하므로 한 유저의 입력 폭주가 다른 방의 입력 큐를 채우지 않습니다. 화면 상단에 배정된 방 번호가 표시됩니다.

HTTP 상태 조회·입장 예약 API와 일회용 입장권을 지원합니다. 계정 인증과 여러 서버를 선택하는 매칭 서비스는 외부 서비스에서 담당하며, 현재 접속 서버의 Tick이 예약과 입장을 확정합니다.

## 구조와 동작

서버 구성 요소, 스레드와 소유권, 입력 처리 및 종료 순서는 [서버 구조 문서](docs/SERVER_ARCHITECTURE.md)에 설명되어 있습니다. 같은 문서의 [파일별 함수 안내](docs/SERVER_ARCHITECTURE.md#파일별-함수-안내)는 파일 이름을 눌러 펼칠 수 있습니다.

서버와 공유 TCP 채널은 `.h`에 선언, 같은 이름의 `.cpp`에 구현을 둡니다. 멤버는 `m_` 접두사, 고정 상수는 `UPPER_SNAKE_CASE`를 사용합니다. `NetworkServer` 별칭은 `NativeServer` 공개 진입점을 가리키며, runtime이 연결·세션·네트워크 스레드를 관리합니다.

```text
Common/       수학, 자체 물리, GameWorld, 입력·Snapshot 자료형, 전송 인터페이스
Server/       실행 진입점, ServerOptions.h/.cpp, ServerApplication.h/.cpp
Server/Core/  ServerHost, GameRoomManager, GameRoom, GameRoomOptions, TickScheduler
Server/Network/ NetworkServer.h, NativeServer/NativeServerNetworkRuntime
Client/       raylib 화면과 키보드, LocalTransport/TcpClientTransport
Assets/       Kenney Food Kit CC0 GLB와 원본 라이선스 고지
Network/      Native/ 이벤트 루프·채널, Protobuf 스키마와 Codec
Tests/        단위 테스트, 루프백, 프로세스 통합 테스트
cmake/        커밋 및 SHA-256으로 고정한 외부 의존성
```

싱글플레이는 같은 프로세스의 ServerCore에 메시지를 전달합니다. 멀티플레이는 Protobuf로 해석한 TCP 입력을 같은 ServerCore의 큐로 전달합니다. 이동과 회전 물리는 Tick 스레드에서만 변경됩니다. 클라이언트는 약 20Hz로 받은 Snapshot의 위치와 자세를 화면에서 보간합니다. 약 50ms의 표시 지연이 추가되며 클라이언트 예측은 하지 않습니다. Protobuf 기반 프로토콜 v8은 이전 버전과 호환되지 않으므로 클라이언트와 서버를 함께 업데이트해야 합니다. 자세한 메시지 규격과 제한은 [PROTOCOL.md](PROTOCOL.md)를 참고하세요.

## 테스트와 제한

- CTest: 벡터/quaternion, 낙하/충돌/구름/관성, 게임 월드, 서버 수명/큐, ServerApplication 실행·종료, 로컬 전송, Protobuf 프로토콜, 화면 보간, CLI, 실제 TCP, 전송 비교, 별도 프로세스 접속. 전체 18개, 외부 의존성 없는 구성 10개입니다. Protobuf 프로토콜 테스트는 네트워크 구성에 포함됩니다.
- 동일 Tick 입력 일정의 로컬 계산과 직렬화 경로는 매 Tick 비교합니다. 실제 비동기 전송은 입력 도착 시점이 다르므로 이동 속도·방향·바닥 정지 등 물리 조건을 비교합니다.
- CI: Windows/MSVC의 네이티브 IOCP와 Linux/GCC의 네이티브 epoll에서 전체 빌드와 테스트, 외부 의존성 없는 코어 구성을 검증합니다. GUI 실행은 CI 필수 항목이 아니며 실제 결과는 [Actions](https://github.com/BaeJiwoo/MoongTangE/actions)에서 확인하세요.
- 자체 구현한 공·복합 구·지형 접촉을 사용합니다. 외부 물리 엔진, 클라이언트 예측, UDP, 물리 롤백 및 사용자 인증은 후속 범위입니다.
- 방마다 최대 3명, 기본 최대 8개 방(24명), 입력 만료 120 Tick, 고정 dt=1/60이며 플랫폼 간 비트 단위의 결정성을 보장하지 않습니다.
- 비공개 저장소 브랜치 보호 설정은 현재 계정 요금제에서 HTTP 403으로 거부됐습니다. 저장소 공개 범위는 변경하지 않았습니다.

## 리뷰와 병합

C++ 전체 가독성·OOP 리뷰 결과와 현재 코드 스타일은 [CPP_REVIEW.md](CPP_REVIEW.md)에 정리했습니다. Islewright를 참고해 멤버 함수는 `Run()`, 일반 함수는 `parse_server_options()`, 비공개 멤버는 `m_networkThread`, 상수는 `FIXED_DELTA`처럼 표기합니다. `.clang-format`과 `.clang-tidy`가 서식·명명 규칙을 검사하며, `python Tools/check_format.py`로 서식을 검증합니다. CTest가 활성화된 빌드는 프로젝트 헤더의 독립 컴파일도 검사합니다.

[ROADMAP.md](ROADMAP.md)의 순서대로 PR을 리뷰하세요. 후속 PR은 선행 미병합 브랜치를 Base로 사용합니다. 병합 승인을 받은 경우, 선행 PR을 merge commit으로 병합한 뒤 다음 PR의 Base를 `main`으로 재지정하고 변경 범위와 검증 결과를 다시 확인합니다. PR 본문에 `Closes #이슈번호`를 포함하면 `main`에 병합할 때 해당 이슈가 자동으로 닫힙니다. 이슈 기록과 작업 브랜치는 보존하며 자동 병합은 활성화하지 않습니다.

의존성과 재배포 시 라이선스 고지는 [THIRD_PARTY.md](THIRD_PARTY.md), 개발 규칙은 [AGENTS.md](AGENTS.md)에 정리되어 있습니다.
