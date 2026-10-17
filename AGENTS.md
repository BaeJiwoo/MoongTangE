# Project development rules

- The service and CMake project name is `MoongTangE`; use it in window titles, on-screen branding and documentation. The repository is `BaeJiwoo/MoongTangE`.

- Follow `CROSS_PLATFORM_3D_PHYSICS_GAME.md` and preserve existing user changes.
- C++20; Common must have no raylib, Asio, or operating system dependencies.
- Physics is implemented here; do not add an external physics engine.
- Only the server tick thread mutates GameWorld. Other threads submit bounded commands.
- Use actual GitHub issue numbers in `<number>-<INITIAL>` branch names.
- Each issue has a separate, reviewable PR. Dependent PRs target the previous work branch.
- Merge PRs only when the user explicitly authorizes it, after reviewing the code and successful checks. For stacked PRs, preserve ancestry with merge commits and retarget the next PR to main after its predecessor merges.
- Never enable auto-merge, push development changes directly to main, force-push, or delete issues. Link each PR with `Closes #<issue>` so merging into main closes its issue automatically.
- Run relevant CMake builds and CTest tests; report unrun and failed checks accurately.
- Follow the C++ Core Guidelines review decisions in `CPP_REVIEW.md`. Prefer cohesive classes and composition; retain simple value types and the transport interface.
- In `Server` and the shared TCP channel, use role-specific class/type names, `m_` prefixes for data members (including option/command value types), and `UPPER_SNAKE_CASE` for named constants and enum values. Runtime local variables and parameters use descriptive lowerCamelCase, including read-only local snapshots. Shared simulation/protocol constants use the same uppercase convention in every caller.
- Keep server and TCP channel declarations in `.h` files and non-template function definitions in matching `.cpp` files. Keep only required constant definitions, default member initializers and defaulted/deleted special members in those headers. Update CMake targets and dependency-free header checks when splitting files.
- Keep `docs/SERVER_ARCHITECTURE.md` synchronized with names, ownership and build targets; document server/shared-network functions in file-specific `<details><summary>` sections.
- Format project C++ with clang-format 22.1.3 and run `Tools/check_format.py`. Keep headers self-contained; run the selected `.clang-tidy` checks for relevant C++ changes.
- New dependencies must use pinned release commits and have documented licenses.
- Do not commit `.tools`, build products, credentials, or downloaded dependency archives.
- DedicatedServer defaults to loopback. Public listening requires an explicit option.
