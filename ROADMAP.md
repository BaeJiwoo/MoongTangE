# Development roadmap

All work is reviewed through issue-specific PRs. Issue numbers below were returned by GitHub. The table records the original stacked bases; during an authorized merge, each PR is retargeted to main after its predecessor merges.

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
| 11 | [#21](https://github.com/BaeJiwoo/MoongTangE/issues/21) | `21-ROLL` | `10-QA` | Rolling inertia/friction, orientation replication and smooth patterned rendering |
| 12 | [#23](https://github.com/BaeJiwoo/MoongTangE/issues/23) | `23-REFACTOR` | `21-ROLL` | Whole-project C++ readability, Core Guidelines and OOP review/refactoring |

Review and merge from top to bottom only with explicit user authorization. Use merge commits to preserve the stack's ancestry. After each predecessor is merged, verify that main matches the predecessor's tree, retarget the next PR to main, and recheck its head commit, CI and issue link. Confirm that merging closes the linked issue; retain issue history and work branches.

Deferred: UDP, client prediction, lag compensation, physics rollback, sphere/sphere collision, and production authentication.

## Follow-up work

| Issue | Branch | PR base | Scope |
| --- | --- | --- | --- |
| #25 | `25-SERVER` | `main` | ServerApplication lifecycle, regression tests and server architecture documentation |
| #26 | `26-PROTOBUF` | `25-SERVER` | Pinned Protobuf, generated network schema, protocol v3 and codec boundary |
| [#29](https://github.com/BaeJiwoo/MoongTangE/issues/29) | `29-NAMING` | `main` | Server names, m_ members, uppercase constants, header/source separation, collapsible function documentation and MoongTangE branding/namespaces/build identifiers across all code |
| [#31](https://github.com/BaeJiwoo/MoongTangE/issues/31) | `31-VSCODE` | `29-NAMING` | Connect CMake headers and compile settings to VS Code Go to Definition |
| [#33](https://github.com/BaeJiwoo/MoongTangE/issues/33) | `33-STYLE` | `31-VSCODE` | Align project-owned C++ names and formatting with Islewright without changing logic |


## Three-player multi-room server (Issues #35–#38)

| Issue | Branch | Dependency | Scope |
| --- | --- | --- | --- |
| [#35](https://github.com/BaeJiwoo/MoongTangE/issues/35) | `35-ROOMS` | #33 | Three-player rooms, oldest-vacancy allocation, lazy creation, empty expiry and reusable spawn slots |
| [#36](https://github.com/BaeJiwoo/MoongTangE/issues/36) | `36-ROOMPROTOCOL` | #35 | Protocol v4 admission result and room identity, client validation and presentation reset |
| [#37](https://github.com/BaeJiwoo/MoongTangE/issues/37) | `37-ROOMRUNTIME` | #36 | Tick-confirmed admission, isolated Local/TCP routing and room CLI settings |
| [#38](https://github.com/BaeJiwoo/MoongTangE/issues/38) | `38-ROOMQA` | #37 | Per-player input coalescing, concurrent admission/cancellation, capacity and separate-process tests |

Rooms hold exactly up to three players and are created only when needed. Default maximum: eight rooms; empty lifetime: 30 seconds. Existing occupants stay in their assigned room. HTTP matchmaking/status integration, reservations, authentication, cross-server scheduling and persistence remain future work. A status snapshot alone must not be treated as a seat reservation in that future integration.
