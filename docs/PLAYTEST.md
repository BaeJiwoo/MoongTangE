# 로컬 플레이테스트

| 확인할 것 | Windows (저장소 루트) | Linux |
| --- | --- | --- |
| 3인 게스트 매칭·게임 | `./Playtest.ps1` | `python3 Tools/playtest.py` |
| 내장 서버에서 공 굴림·부착 | `./Playtest.ps1 single` | `python3 Tools/playtest.py single` |
| 빠른 회귀 검사 | `./Playtest.ps1 check` | `python3 Tools/playtest.py check` |
| 1인 매칭 | `./Playtest.ps1 --clients 1` | `python3 Tools/playtest.py --clients 1` |

- 최초 실행: [README의 빌드 도구](../README.md#빌드) 설치 필요. CMake가 의존성을 준비하므로 오래 걸릴 수 있음. 이후에는 증분 빌드.
- Windows 진입점은 기존 CMake의 Python 경로 → `py -3` → PATH의 Python 순서로 찾음. Python을 직접 지정해 `Tools/playtest.py`를 실행해도 동일하게 동작.
- `check`: 외부 그래픽·네트워크 의존성 없이 `build/playtest-core`에서 `PhysicsTests`, `WorldTests`, `SurvivalTests`, `RenderingTests`, `CliTests`, `LocalTests`, `PlaytestToolTests` 실행. 전체 통합 테스트를 대체하지 않음.
- `play`/`single`: 해당 운영체제의 Release 프리셋 사용. `play`는 API도 빌드. 기본 3명, 최대 3명. 서버 한 개·방 한 개로 실제 게스트 세션 → 예약 → TCP 입장 경로 사용.
- `--seed 42`: 초기 물체 배치 재현. 기본 seed 42. 입력 시점까지 재현하는 리플레이 기능은 아님.
- `--skip-build`: 현재 빌드 결과를 그대로 사용. 소스와 실행 파일이 다를 수 있다는 안내 표시. 기본 실행은 빌드를 생략하지 않음.
- `--smoke`: 숨겨진 창에서 자동 이동·회전을 검증하고 종료. OpenGL 환경 필요. 예: `./Playtest.ps1 --smoke`.

## 조작·종료·로그

| 동작 | 방법 |
| --- | --- |
| 이동 | 조작할 창을 선택한 뒤 WASD |
| 복합 콜라이더·접촉 법선 확인 | F3 |
| 메뉴 | Esc |
| 사망 후 다시 플레이 | Fail 화면에서 PLAY AGAIN 또는 Enter/R |
| 사망 후 메인 메뉴 | Fail 화면에서 MAIN MENU 또는 Esc |
| 전체 종료 | 게임 창 모두 닫기 또는 실행 터미널 Ctrl+C |
| 로그 확인 | 실행 시 출력되는 `build/playtest-logs/<시각>-<고유값>` |

각 클라이언트는 `Playtest1`~`Playtest3` 제목·닉네임으로 1100×720 창에서 시작합니다.
`--playtest <nickname>` 옵션이 메뉴를 건너뛰고 기본 창 설정을 사용합니다. 기존 설정 파일은
자동으로 덮어쓰지 않습니다. 게임 안에서 OPTIONS → KEEP으로 저장하면 평소처럼 설정이 저장됩니다.

`api.log`, `server.log`, `client-1.log` 등에서 프로세스별 오류를 확인할 수 있습니다.
클라이언트의 첫 Snapshot 수신 시 방·플레이어 ID·닉네임을 기록하며, 실행 도구가 모두 같은 방의
서로 다른 플레이어인지 확인합니다. 실행 도구는 자신이 만든 프로세스 객체만 보유하고 정리합니다.
API·서버는 루프백·임시 포트를 사용하고 임시 서버 키를 생성합니다. 키는 서버 환경으로만 전달하며
클라이언트 환경에서는 제거합니다. 로그·상태 파일에 키를 쓰지 않습니다.

현재 자동 검사의 통과가 공 굴림·부착의 체감 품질까지 보장하지는 않습니다. 부착 순간의 표시 보간과
부착 후 이동성 문제는 별도 수정 대상입니다. 수동 확인 시 seed, 조작 방향, 부착 물체와 F3 화면을
함께 남기면 같은 상황을 비교하기 쉽습니다.
