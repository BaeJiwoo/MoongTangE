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
| 11 | [#21](https://github.com/BaeJiwoo/portable-physics-sandbox/issues/21) | `21-ROLL` | `10-QA` | Rolling inertia/friction, orientation replication and smooth patterned rendering |
| 12 | [#23](https://github.com/BaeJiwoo/portable-physics-sandbox/issues/23) | `23-REFACTOR` | `21-ROLL` | Whole-project C++ readability, Core Guidelines and OOP review/refactoring |

Review and merge from top to bottom only with explicit user authorization. Use merge commits to preserve the stack's ancestry. After each predecessor is merged, verify that main matches the predecessor's tree, retarget the next PR to main, and recheck its head commit, CI and issue link. Confirm that merging closes the linked issue; retain issue history and work branches.

Deferred: UDP, client prediction, lag compensation, physics rollback, sphere/sphere collision, and production authentication.

## Follow-up work

| Issue | Branch | PR base | Scope |
| --- | --- | --- | --- |
| #25 | `25-SERVER` | `main` | ServerApplication lifecycle, regression tests and server architecture documentation |
| #26 | `26-PROTOBUF` | `25-SERVER` | Pinned Protobuf, generated network schema, protocol v3 and codec boundary |
