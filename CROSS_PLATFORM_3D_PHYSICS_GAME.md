# MoongTangE 작업 지시서: Cross-Platform 3D Physics Game

> 서비스 이름 갱신 (2026-10-17): 서비스·CMake 프로젝트·게임 창·화면 제목과 모든 C++ 코드의 최상위 네임스페이스는 `MoongTangE`로 통일합니다. Protobuf 패키지는 `MoongTangE.proto`, 빌드 옵션·매크로·환경변수는 `MOONGTANGE_`, CMake 내부 도우미는 `moongtange_`를 사용합니다. Client·Common·Network·Server·Tests와 CI·검사 도구·문서에 모두 적용합니다. GitHub 저장소는 `BaeJiwoo/MoongTangE`입니다.

> 플랫폼·네트워크 지시 갱신 (2026-10-27, Issue #67): 지원 운영체제는 Windows와 Linux로 한정하고 macOS는 개발·빌드·CI 대상에서 제외합니다. 현재 브랜치 계열은 CMake의 `MOONGTANGE_NETWORK_BACKEND=AUTO|IOCP|EPOLL`만 제공하며, 기본 `AUTO`는 Windows의 직접 구현 IOCP 또는 Linux의 직접 구현 epoll을 사용합니다. 이전 Standalone Asio 구현은 별도 `61-NETWORKBACKEND` 브랜치 계열에 보존합니다. 아래 최초 지시서의 macOS 지원 및 네이티브 IOCP/epoll 구현 금지 문구는 원문 기록으로만 유지하며 이 갱신이 우선합니다.

> 서버 코드 정리 규칙 (2026-10-17, Issue #29): 서버와 공유 TCP 채널의 모호한 구현체 이름은 역할을 나타내는 이름으로 바꿉니다. 멤버 변수는 `m_`, 고정 상수·열거 값은 `UPPER_SNAKE_CASE`를 사용합니다. 런타임 지역 변수·매개변수는 의미 있는 lowerCamelCase를 사용합니다. `.h`에는 선언을, 대응하는 `.cpp`에는 비템플릿 함수 구현을 둡니다. 상수 정의·멤버 기본값·default/delete 선언은 헤더에 유지합니다. 공유 시뮬레이션·프로토콜 상수 변경은 모든 호출부에 반영합니다. 서버 아키텍처에는 파일별 함수 설명을 `<details><summary>` 토글로 제공합니다.

> 운영 지시 갱신 (2026-10-17): 사용자가 후속 요청으로 코드 리뷰와 PR 병합을 명시적으로 지시했습니다. 이번 기존 PR 병합 작업에는 이 최신 요청을 적용합니다. 아래 최초 지시서의 병합 금지 문구는 원문 기록으로 보존합니다. 향후 병합도 명시적인 사용자 승인이 필요하며, 자동 병합·main 직접 개발 Push·강제 Push·이슈 삭제는 허용하지 않습니다.

## 0. 역할 및 최종 목표

당신은 C++ 게임 엔진 및 게임 서버 개발자이자 GitHub 프로젝트 관리자입니다.

이번 작업에서는 GitHub 저장소를 실제로 구성하고, 개발 작업을 GitHub Issues로 세분화한 뒤, 각 이슈마다 별도 브랜치에서 구현하고 Pull Request를 생성해야 합니다.

### 최종 목표

- C++20 기반 3D 물리 시뮬레이션 프로젝트 구현
- Windows, Linux, macOS에서 빌드 가능한 구조
- Client / Common / Server 모노레포 구성
- Common에 자체 구현 물리 엔진 배치
- 싱글플레이에서 내부 서버(Embedded Server) 실행
- 멀티플레이에서 독립 서버(Dedicated Server) 실행
- 간단한 평면 위 공을 WASD로 이동
- 물리 계산은 서버 권위 방식으로 처리
- GitHub Issue, Branch, PR 기반 개발 프로세스 적용

**가장 중요한 규칙: 모든 작업을 PR로 등록하고, 절대 병합하지 마세요.**

최종 코드 병합과 승인 권한은 사용자에게 있습니다.

---

## 1. GitHub 저장소 설정

우선 현재 작업 디렉터리와 Git 설정을 검사하세요.

1. Git 저장소 존재 여부 확인
2. GitHub 원격 저장소 연결 확인
3. `gh auth status`로 GitHub CLI 인증 확인
4. 기존 프로젝트 파일이 있는지 확인
5. 기존 저장소가 유효하다면 재사용
6. 저장소가 없다면 신규 비공개 저장소 생성

신규 저장소 기본값:

- Repository: `MoongTangE`
- Visibility: Private
- Default Branch: `main`
- Description: `Cross-platform C++ 3D physics game with embedded and dedicated server support`

GitHub 저장소 생성에는 `gh repo create`를 사용하세요.

저장소의 초기 README 생성 이후 모든 개발 변경은 작업 브랜치와 PR을 통해 진행합니다.

기존 파일이나 Git 기록을 임의로 삭제하지 마세요. 인증이나 권한 문제로 GitHub 작업이 불가능하면 우회하지 말고 정확한 문제를 보고하세요.

프로젝트 관리 파일도 준비하세요.

- `README.md`
- `AGENTS.md`
- `ROADMAP.md`
- `.gitignore`
- `.github/workflows/ci.yml`
- `.github/PULL_REQUEST_TEMPLATE.md`

가능하다면 `main` 직접 Push를 제한하는 브랜치 규칙도 설정하세요. 사용자의 승인 없이 자동 병합되는 규칙은 만들지 마세요.

## 2. 기술 스택

다음 기술을 사용하세요.

| 영역 | 기술 |
| --- | --- |
| Language | C++20 |
| Build System | CMake |
| Networking | Standalone Asio |
| Rendering | raylib |
| Testing | CTest 기반 자동 테스트 |
| CI | GitHub Actions |
| Version Control | Git / GitHub CLI |

의존성은 검증된 안정 릴리스에 고정하세요. 초기 기준으로 Standalone Asio 1.38.2 및 raylib 6.0을 검토하고, 실제 적용 가능한 태그와 빌드 방식을 확인하세요.

외부 라이브러리를 사용해도 되지만 물리 계산에는 외부 물리 엔진을 사용하면 안 됩니다.

다음 원칙을 지키세요.

- Windows 전용 IOCP를 직접 구현하지 않습니다.
- Linux 전용 epoll API를 직접 사용하지 않습니다.
- 시스템별 소켓 API 대신 Asio를 이용합니다.
- 플랫폼별 구현은 외부 라이브러리와 빌드 설정 안에 격리합니다.
- 코어 코드가 특정 운영체제에 의존하지 않도록 설계합니다.
- 서버의 물리 시뮬레이션에는 렌더링 의존성을 추가하지 않습니다.
- 필수 외부 의존성과 라이선스를 문서화합니다.

네트워크는 우선 TCP로 구현합니다. 향후 UDP 전송 구현체를 추가할 수 있도록 인터페이스를 설계하세요.

## 3. 모노레포 아키텍처

다음 구조를 기본으로 사용하세요.

```text
MoongTangE/
├── CMakeLists.txt
├── README.md
├── AGENTS.md
├── ROADMAP.md
├── Common/
│   ├── Math/
│   │   └── Vector3.h
│   ├── Physics/
│   │   ├── PhysicsWorld.h
│   │   ├── RigidBody.h
│   │   └── Collision/
│   ├── Game/
│   │   ├── GameWorld.h
│   │   └── PlayerController.h
│   ├── Protocol/
│   │   ├── InputCommand.h
│   │   └── WorldSnapshot.h
│   └── Transport/
│       └── IClientTransport.h
├── Server/
│   ├── Core/
│   │   ├── ServerHost.h
│   │   └── TickScheduler.h
│   ├── Network/
│   │   └── AsioServer.h
│   └── main.cpp
├── Client/
│   ├── main.cpp
│   ├── Rendering/
│   ├── Input/
│   └── Transport/
│       ├── LocalTransport.h
│       └── TcpClientTransport.h
└── Tests/
```

실제 구현에 따라 파일은 추가하거나 조정할 수 있습니다.

### Common

Common은 순수 C++ 라이브러리입니다.

담당 기능:

- Vector3 등 수학 자료형
- PhysicsWorld
- RigidBody
- 중력, 가속도, 속도 및 위치 계산
- Sphere와 Plane 충돌
- GameWorld
- PlayerController
- InputCommand와 WorldSnapshot
- 전송 계층 인터페이스

Common은 raylib이나 Asio에 의존하지 않아야 합니다.

### Server

Server는 게임 월드를 실행하는 호스트 역할을 담당합니다.

서버 코어와 독립 실행 프로그램을 분리하세요.

- `ServerCore`: 재사용 가능한 Tick 및 월드 실행 라이브러리
- `DedicatedServer`: ServerCore를 사용하는 독립 실행 파일

ServerCore는 Client 내부에서도 사용할 수 있어야 합니다.

### Client

Client는 다음 기능을 담당합니다.

- raylib을 통한 3D 렌더링
- 키보드 입력
- 게임 상태 표시
- LocalTransport 및 TcpClientTransport
- 싱글플레이 시 ServerCore 실행

Client가 서버 권위 물리 상태를 임의로 수정해서는 안 됩니다.

## 4. 싱글플레이 및 멀티플레이 구조

### 싱글플레이

1. Client가 실행됩니다.
2. 프로세스 내부에 Embedded Server를 생성합니다.
3. 별도 서버 실행 루프에서 GameWorld를 구동합니다.
4. Client가 WASD 입력을 LocalTransport로 전달합니다.
5. Embedded Server가 입력을 소비합니다.
6. Common의 PhysicsWorld가 물리를 계산합니다.
7. 계산된 WorldSnapshot을 Client에 전달합니다.
8. Client가 결과를 렌더링합니다.

싱글플레이에서는 소켓 통신을 강제하지 않습니다.

프로세스 내부 메시지 큐를 사용할 수 있습니다.

### 멀티플레이

1. DedicatedServer가 별도 프로세스로 실행됩니다.
2. Asio 기반 TCP 서버를 시작합니다.
3. Client가 TCP로 연결합니다.
4. Client가 입력 명령을 전송합니다.
5. 서버가 입력을 검증하고 GameWorld에 적용합니다.
6. PhysicsWorld가 위치 및 충돌을 계산합니다.
7. 서버가 WorldSnapshot을 전송합니다.
8. Client가 수신된 상태를 렌더링합니다.

두 모드는 동일한 GameWorld, PhysicsWorld 및 ServerCore를 사용해야 합니다.

네트워크 스레드는 물리 상태를 직접 수정하지 않고, 입력 큐를 통해 Tick 실행 스레드에 전달해야 합니다.

## 5. 첫 번째 데모 요구사항

최소한 다음 기능이 정상적으로 동작해야 합니다.

### 월드

- Y=0에 고정된 평면 생성
- 반지름 0.5인 Sphere 생성
- 초기 위치 (0, 2, 0)
- 중력 (0, -9.81, 0)
- 바닥 충돌 판정
- 공이 바닥을 통과하지 않도록 처리

### 입력

- W: 전진
- S: 후진
- A: 왼쪽 이동
- D: 오른쪽 이동
- 대각선 이동 속도 정규화

### 시뮬레이션

- 물리 Tick 60Hz
- WorldSnapshot 기본 송신 20Hz
- 고정 시간 간격 사용
- 서버에서 이동 및 충돌 처리
- 정지 상태의 공이 안정적으로 바닥에 유지

### 화면

- 3D 카메라
- 평면과 공 표시
- WASD 조작 결과 반영
- 현재 FPS와 서버 모드 표시

가능하면 아래 실행 방법을 제공하세요.

- `Client --single`
- `Client --connect 127.0.0.1:27015`
- `DedicatedServer --port 27015`

명령행 옵션은 플랫폼 간 동일하게 동작해야 합니다.

DedicatedServer는 기본적으로 루프백 인터페이스에 바인딩하세요. 외부 연결 허용은 명시적 옵션으로 분리하고 인증되지 않은 테스트 서버의 외부 공개 위험을 문서화하세요.

## 6. 네트워크 프로토콜 요구사항

초기 프로토콜에는 최소한 다음 메시지를 구현하세요.

- ClientHello / ServerHello
- InputCommand
- WorldSnapshot
- Disconnect 또는 오류 처리

InputCommand에는 입력 시퀀스 번호가 포함되어야 합니다.

WorldSnapshot에는 서버 Tick과 공의 위치 및 속도가 포함되어야 합니다.

다음 사항도 고려하세요.

- TCP 메시지 프레이밍
- 명시적인 바이트 순서
- 부분 수신 및 부분 송신
- 최대 메시지 크기 제한
- 프로토콜 버전
- 잘못된 패킷 거부
- 비정상 연결 종료 및 자원 정리
- NaN, Infinity 및 비정상 입력값 검증

C++ 구조체 메모리를 그대로 네트워크에 전송하지 마세요. 명시적인 직렬화와 역직렬화를 구현해야 합니다.

초기 단계에서는 고급 클라이언트 예측, Lag Compensation 및 복잡한 물리 롤백을 구현하지 않습니다.

## 7. GitHub Issues 작업 계획

아래 항목을 최소 개발 단위로 사용하세요.

각 이슈는 담당 범위, 의존 이슈, 작업 항목, Acceptance Criteria 및 테스트 방법을 포함해야 합니다.

| 순서 | 이슈 제목 | 브랜치 이니셜 |
| --- | --- | --- |
| 1 | CMake 모노레포 및 프로젝트 초기 구성 | INIT |
| 2 | Vector3 수학 라이브러리 구현 | MATH |
| 3 | PhysicsWorld, 중력 및 바닥 충돌 구현 | PHYS |
| 4 | GameWorld, 입력 및 Snapshot 설계 | WORLD |
| 5 | ServerCore 및 Fixed Tick 구현 | TICK |
| 6 | Transport 인터페이스와 LocalTransport 구현 | LOCAL |
| 7 | raylib 클라이언트 및 Embedded Server 구현 | CLIENT |
| 8 | Standalone Asio 네트워크 및 프로토콜 구현 | NET |
| 9 | DedicatedServer 및 원격 Client 연결 | MULTI |
| 10 | 통합 테스트, CI 및 실행 문서 완성 | QA |

각 이슈는 하나의 명확한 책임을 갖도록 구성하세요. 필요하다면 추가 이슈로 더 작게 분리해도 됩니다.

**작업은 위 순서를 기반으로 진행하되, 의존성이 있는 구현을 억지로 독립된 브랜치에서 개발하지 마세요.**

## 8. 브랜치 및 PR 운영 규칙

다음 워크플로를 반드시 준수하세요.

### 8.1 이슈 등록

GitHub CLI로 이슈를 실제 생성하세요.

예시:

```bash
gh issue create --title "Implement Vector3 math library" --body-file issue.md
```

이슈가 생성되면 반환된 실제 이슈 번호를 사용하세요.

이슈 번호를 임의로 가정하지 마세요.

### 8.2 브랜치 생성

브랜치 이름은 다음 규칙을 따르세요.

`<실제 이슈 번호>-<간단한 이니셜>`

예시:

- `12-INIT`
- `13-MATH`
- `14-PHYS`
- `15-WORLD`

번호는 예시일 뿐입니다. GitHub에서 실제 생성된 번호를 사용하세요.

### 8.3 개발

각 브랜치에서 해당 이슈에 필요한 기능만 개발하세요.

- 코드 작성
- 관련 단위 테스트 추가
- 로컬 빌드
- 테스트 실행
- 오류 수정
- Git Commit
- 원격 브랜치 Push

가능한 경우 테스트 결과를 PR에 남기세요.

### 8.4 PR 생성

`gh pr create`를 이용하세요.

PR 제목 예시:

`[13] Implement Vector3 math operations`

PR 본문에는 다음 내용을 포함하세요.

- Related Issue: `#13`
- 변경 사항 요약
- 주요 설계 결정
- 테스트 및 빌드 결과
- 확인이 필요한 사항
- 선행 PR 또는 의존성
- 리뷰 체크리스트

Issue 자동 종료 연결은 PR 병합 시점에만 동작하도록 `Closes #번호` 형식을 사용해도 됩니다.

### 8.5 Stacked PR 정책

**중요: 모든 PR을 병합하지 않은 상태로 유지해야 합니다.**

첫 번째 PR은 `main`을 대상으로 생성하세요.

후속 PR이 이전 이슈의 미병합 코드에 의존한다면 이전 작업 브랜치를 Base로 지정하세요.

예시:

- PR 1: `12-INIT` → `main`
- PR 2: `13-MATH` → `12-INIT`
- PR 3: `14-PHYS` → `13-MATH`
- PR 4: `15-WORLD` → `14-PHYS`

이후 PR도 같은 방식으로 진행합니다.

독립적인 이슈는 `main`을 대상으로 별도 PR을 생성할 수 있습니다.

Stacked PR은 각 PR이 해당 이슈의 변경 사항만 보여주도록 구성해야 합니다.

서로 의존하는 PR은 리뷰 과정에서 병합 순서를 명확히 안내하세요.

선행 PR이 병합된 이후 후속 PR의 Base가 적절한지 확인할 필요가 있다는 점도 문서화하세요.

### 절대 수행하면 안 되는 작업

- `gh pr merge`
- PR 자동 병합 활성화
- PR 승인 없이 main에 직접 Push
- 강제 Push로 기존 Git 기록 훼손
- 사용자 승인 없이 PR 종료 또는 이슈 삭제
- 테스트가 실패했는데 성공했다고 보고

## 9. 테스트 및 품질 기준

테스트는 최소한 다음을 포함해야 합니다.

### 수학 테스트

- Vector3 덧셈과 뺄셈
- 스칼라 곱
- 길이와 정규화
- 영벡터 처리

### 물리 테스트

- 정지 상태의 공이 중력으로 낙하
- Sphere와 Plane 충돌
- 공이 바닥 아래로 침투하지 않음
- 고정 Tick에 따른 위치 계산
- 동일 입력 시 동일 플랫폼에서 허용 오차 내 결과 재현

### 서버 테스트

- ServerCore 시작과 종료
- InputCommand 처리
- Snapshot 생성
- LocalTransport 메시지 전달
- 입력 큐와 서버 Tick 분리

### 네트워크 테스트

- 루프백 접속
- 입력 패킷 송수신
- 프레이밍 및 직렬화
- 부분 수신
- 비정상 패킷 처리
- 연결 종료
- LocalTransport와 TCP Transport의 시뮬레이션 결과 비교

### 플랫폼 테스트

GitHub Actions에서 다음 빌드 매트릭스를 구성하세요.

- Windows / MSVC
- Linux / GCC
- macOS / Clang

최소한 Common, ServerCore, DedicatedServer 및 자동 테스트를 모든 대상 플랫폼에서 검증하세요.

raylib GUI 테스트는 환경에 따라 별도 처리할 수 있지만, 빌드 성공 여부는 검증하고 제한 사항을 문서화하세요.

실행하지 않은 테스트를 통과한 것처럼 작성하지 마세요.

## 10. Codex 작업 수행 원칙

이 작업은 단순한 설계 문서 작성이 아니라 실제 구현 작업입니다.

다음 순서로 실행하세요.

1. 현재 작업 환경과 GitHub 인증 확인
2. 저장소 생성 또는 기존 저장소 연결
3. 프로젝트 구조 및 작업 계획 수립
4. 전체 개발 이슈 등록
5. 첫 번째 이슈 브랜치 생성
6. 실제 코드 구현 및 테스트
7. Commit, Push 및 PR 생성
8. 다음 이슈로 진행
9. 필요한 의존 관계에 따라 Stacked PR 생성
10. 최종 구현 상태 및 PR 목록 보고

모든 이슈는 가능한 한 실제 구현까지 완료하고 PR을 등록하세요.

단, 작업 환경이나 외부 권한 때문에 진행이 불가능하다면 우회하거나 결과를 꾸미지 말고 중단 사유와 완료된 범위를 명확하게 보고하세요.

작업 중 기존 PR과 브랜치가 존재한다면 중복 생성하지 말고 현재 상태를 파악한 후 이어서 진행하세요.

현재 작업과 무관한 파일을 수정하지 마세요.

이슈 하나가 지나치게 커지면 세분화하세요.

각 PR은 사람이 독립적으로 리뷰할 수 있는 크기를 유지하는 것을 우선합니다.

## 11. 최종 결과 보고 형식

모든 작업을 수행한 후 한국어로 결과를 보고하세요.

### 저장소

- GitHub Repository URL
- 사용한 기술 스택
- 주요 디렉터리 구조

### Issues & PRs

| Issue | Branch | PR URL | Base Branch | Status |
| --- | --- | --- | --- | --- |
| 실제 번호 | 실제 이름 | 실제 URL | 실제 대상 | 구현/테스트 상태 |

### 구현 결과

- 싱글플레이 실행 여부
- 멀티플레이 실행 여부
- WASD 이동 구현 여부
- 물리 시뮬레이션 구현 여부
- 플랫폼별 빌드 결과

### 실행 방법

- 빌드 명령어
- 싱글플레이 실행 명령어
- 서버 실행 명령어
- 멀티플레이 클라이언트 접속 방법

### 사용자 확인 필요 사항

- PR 병합 권장 순서
- 미완료 기능
- 테스트 실패 또는 검증하지 못한 환경
- 향후 구현을 권장하는 기능

**최종 상태는 모든 구현 PR이 Open 상태로 남아 있어야 합니다.**

사용자가 리뷰하고 직접 병합할 수 있도록 유지하세요.

지금부터 GitHub 저장소 상태를 확인하고 실제 작업을 시작하세요. 단순히 계획을 설명하는 데서 끝내지 마세요.
