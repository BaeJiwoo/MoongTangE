# 분리형 매칭 API 연동 — Issues #43, #51

매칭 API와 게임 서버는 서로 다른 실행 파일이며 다른 컴퓨터에 배포할 수 있습니다.

| 프로세스 | 구현 | 수신 연결 | 책임 |
| --- | --- | --- | --- |
| 매칭 API | ASP.NET Core 8 (MatchmakingApi) | HTTP | 게임 서버 목록, 공개 예약 API, 제어 명령 보관 |
| 게임 서버 | C++ (DedicatedServer) | 게임 TCP | 60Hz Tick, 방·물리, 최종 예약 판단과 입장권 소비 |

C++ 서버에는 HTTP 수신 포트가 없습니다. 주기적으로 API에 outbound HTTP heartbeat를 보내고 그 응답에서 예약·취소 명령을 가져옵니다. API가 만든 예약은 C++ Tick이 자리를 확보했다고 보고한 뒤에만 reserved가 됩니다. API 장애 중에도 이미 접속한 게임과 Tick은 계속 동작합니다.

## 요청 흐름

1. C++ 게임 서버가 ASP.NET API에 공개 게임 주소와 점유 상태를 등록합니다.
2. 클라이언트 백엔드가 공개 API에 256비트 난수 입장권으로 예약을 요청합니다.
3. C++ 제어 스레드가 다음 heartbeat 응답에서 reserve 명령을 가져와 제한된 Tick 명령 큐에 넣습니다.
4. Tick이 정원을 확인해 자리를 확보하고, C++ 제어 스레드가 결과를 API에 보고합니다.
5. API는 reserved 결과에 게임 주소·방·입장권을 담아 반환합니다.
6. 게임 클라이언트는 C++ TCP 서버에 닉네임과 입장권을 보내며 Tick이 입장권을 한 번만 소비합니다.

## 로컬에서 두 서비스 실행

.NET 8 SDK와 CMake 빌드 결과가 필요합니다. 두 키는 최소 32자의 출력 가능한 ASCII이며 서로 다른 용도로 사용합니다.

    export MOONGTANGE_PUBLIC_API_KEY="$(python3 -c 'import secrets; print(secrets.token_hex(32))')"
    export MOONGTANGE_CONTROL_KEY="$(python3 -c 'import secrets; print(secrets.token_hex(32))')"

    # 터미널 1: 독립 ASP.NET 프로세스
    ASPNETCORE_URLS=http://127.0.0.1:27016 \
    dotnet run --project MatchmakingApi/MoongTangE.MatchmakingApi.csproj -c Release

    # 터미널 2: 독립 C++ 프로세스
    ./build/DedicatedServer --port 27015 --require-ticket \
      --matchmaking-url http://127.0.0.1:27016 \
      --server-id local-1 --advertise-host 127.0.0.1

    # 터미널 3: 공개 API를 사용하는 예제 백엔드
    python3 Tools/matchmaking_client.py --api http://127.0.0.1:27016 --client build/Client

다른 컴퓨터에 배포할 때 --matchmaking-url에는 C++ 서버에서 접근 가능한 API 주소를, --advertise-host에는 실제 게임 클라이언트가 접근 가능한 C++ 서버 주소를 넣고 게임 서버에는 명시적으로 --public을 추가합니다. 방화벽은 API의 HTTP 포트와 게임 서버의 TCP 포트를 각각 필요한 주체에만 엽니다.

| 설정 | 소유 프로세스 | 의미 |
| --- | --- | --- |
| MOONGTANGE_PUBLIC_API_KEY | ASP.NET, 신뢰하는 클라이언트 백엔드 | 공개 상태·예약 API 인증 |
| MOONGTANGE_CONTROL_KEY | ASP.NET, C++ 게임 서버 | 내부 등록·명령·결과 API 인증 |
| --matchmaking-url URL | C++ | ASP.NET API origin. 현재 http:// 지원 |
| --server-id ID | C++ | 배포 내 고유 서버 ID |
| --advertise-host HOST | C++ | API가 클라이언트에 돌려줄 게임 주소 |
| --require-ticket | C++ | 입장권 없는 TCP 접속 거부. 매칭 URL 필요 |
| --reservation-seconds N | C++ | Tick이 확보한 자리의 만료 시간, 기본 30초 |

현재 내부 연결은 평문 HTTP입니다. 호스트가 분리되면 사설망이나 mTLS/TLS 프록시를 사용하고 인증 헤더를 로그에 남기지 않습니다. 게임 TCP도 별도 암호화가 없으므로 공용 인터넷 배포에는 보안 터널이 필요합니다.

## 공개 HTTP API

Authorization: Bearer <MOONGTANGE_PUBLIC_API_KEY>가 필요하고 응답은 Cache-Control: no-store입니다.

- GET /health: 프로세스 생존 확인(인증 없음).
- GET /v1/status: 최근 3초 안에 heartbeat를 보낸 게임 서버 목록.
- PUT /v1/reservations: 신규 예약 또는 같은 입장권의 기존 결과.
- GET /v1/reservations: 예약 상태 조회.
- DELETE /v1/reservations: 미소비 예약 취소.

예약 요청에는 X-Admission-Ticket: <64자리 소문자 hex>를 보냅니다. 입장권은 bearer 자격 증명이자 멱등 키이므로 재사용하거나 로그에 기록하지 않습니다.

    {
      "status": "reserved",
      "server_id": "local-1",
      "game_host": "127.0.0.1",
      "game_port": 27015,
      "protocol_version": 6,
      "room_id": "1",
      "expires_in_ms": 29834
    }

pending은 202, reserved·consumed는 200, full은 409, expired·cancelled는 410, 사용 가능한 서버가 없거나 배정 서버 heartbeat가 끊기면 503입니다. 클라이언트에는 reserved 결과만 전달합니다.

## 내부 게임 서버 API

Authorization: Bearer <MOONGTANGE_CONTROL_KEY>가 필요합니다. C++ 전용 계약이며 일반 게임 클라이언트에 노출하지 않습니다.

- PUT /internal/v1/game-servers/{serverId}: 주소·프로토콜·정원을 헤더로 등록/갱신하고 TSV 명령 목록을 받습니다.
- PUT /internal/v1/game-servers/{serverId}/commands/{commandId}: Tick이 처리한 예약/취소 결과를 보고합니다.
- PUT /internal/v1/game-servers/{serverId}/reservations/{ticket}: 이후 소비·만료 상태 변화를 보고합니다.

게임 상태 변경은 제어 HTTP 스레드가 직접 하지 않습니다. MatchmakingControlClient는 ServerHost의 제한된 명령 큐를 사용하며 실제 정원 판단·소비·취소·만료는 Tick에서 직렬 처리됩니다.

MatchmakingApiSmoke는 ASP.NET과 C++를 서로 다른 프로세스로 시작해 등록, 인증, 멱등 예약, Tick 확정, TCP 입장·소비, 취소, 만료, heartbeat 단절을 검증합니다. API 저장소는 현재 단일 프로세스 메모리 구현이므로 API 재시작 시 등록·예약 기록은 사라집니다.

API 저장소는 게임 서버 1,024개, 예약 결과 4,096개, 서버별 미처리 명령 1,024개로 제한합니다. 종료 상태 기록과 연결이 끊긴 빈 서버 기록은 60초 뒤 정리합니다. C++ 코어의 실제 정원과 명령 큐 상한은 [서버 구조](SERVER_ARCHITECTURE.md)에 설명합니다.
