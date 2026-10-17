# Project development rules

- The service, CMake project and C++ root namespace are `MoongTangE`. The Protobuf package is `MoongTangE.proto`; project macros, build options and environment variables use `MOONGTANGE_`, and CMake helper names use `moongtange_`. Apply these names across Client, Common, Network, Server, Tests, tools, CI and documentation. The repository is `BaeJiwoo/MoongTangE`.

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
- Follow the Islewright naming and formatting conventions documented in `CPP_REVIEW.md` across all project-owned C++: PascalCase types and member functions, snake_case free functions, `m_` + lowerCamelCase private data members, and `UPPER_SNAKE_CASE` named constants and enum values. Use role-specific class/type names. Runtime local variables and parameters use descriptive lowerCamelCase, including read-only local snapshots. Keep simple math/protocol value fields unprefixed; retain the existing `m_` fields of server option/command value types. External APIs and generated Protobuf names follow their own conventions.
- Group related declarations and statements together. Use one blank line between responsibilities or processing stages (validation, setup, execution, cleanup), and keep one operation's calculations, state updates and associated checks together. Preserve declaration/statement order when changing spacing. The formatter preserves these semantic groups; it cannot infer them.
- Keep server and TCP channel declarations in `.h` files and non-template function definitions in matching `.cpp` files. Keep only required constant definitions, default member initializers and defaulted/deleted special members in those headers. Update CMake targets and dependency-free header checks when splitting files.
- Keep `docs/SERVER_ARCHITECTURE.md` synchronized with names, ownership and build targets; document server/shared-network functions in file-specific `<details><summary>` sections.
- Format project C++ with clang-format 22.1.3 and run `Tools/check_format.py`. Keep headers self-contained; run the selected `.clang-tidy` checks for relevant C++ changes.
- New dependencies must use pinned release commits and have documented licenses.
- Do not commit `.tools`, build products, credentials, or downloaded dependency archives.
- DedicatedServer defaults to loopback. Public listening requires an explicit option.
