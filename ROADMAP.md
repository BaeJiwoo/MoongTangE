# Development roadmap

All work is reviewed through open, unmerged PRs. Issue numbers below were returned by GitHub.

| Order | Issue | Branch | PR base | Scope |
| --- | --- | --- | --- | --- |
| 1 | #1 | `1-INIT` | `main` | CMake, CI and project rules |
| 2 | #2 | `2-MATH` | `1-INIT` | Vector3 |
| 3 | #3 | `3-PHYS` | `2-MATH` | Gravity and sphere/plane collision |
| 4 | #4 | `4-WORLD` | `3-PHYS` | GameWorld, input and snapshots |
| 5 | #5 | `5-TICK` | `4-WORLD` | ServerCore and fixed tick |
| 6 | #6 | `6-LOCAL` | `5-TICK` | Local transport |
| 7 | #7 | `7-CLIENT` | `6-LOCAL` | raylib and embedded server |
| 8 | #8 | `8-NET` | `7-CLIENT` | Wire protocol and TCP |
| 9 | #9 | `9-MULTI` | `8-NET` | Dedicated server and remote client |
| 10 | #10 | `10-QA` | `9-MULTI` | Integration, CI and documentation |

Review and merge from top to bottom. After each predecessor is merged, check and retarget the next PR base as appropriate. Only the user merges.

Deferred: UDP, client prediction, lag compensation, physics rollback, sphere/sphere collision, and production authentication.
