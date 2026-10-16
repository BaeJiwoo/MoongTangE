# Project development rules

- Follow `CROSS_PLATFORM_3D_PHYSICS_GAME.md` and preserve existing user changes.
- C++20; Common must have no raylib, Asio, or operating system dependencies.
- Physics is implemented here; do not add an external physics engine.
- Only the server tick thread mutates GameWorld. Other threads submit bounded commands.
- Use actual GitHub issue numbers in `<number>-<INITIAL>` branch names.
- Each issue has a separate, reviewable PR. Dependent PRs target the previous work branch.
- Never merge PRs, enable auto-merge, push development changes to main, force-push, or delete issues.
- Run relevant CMake builds and CTest tests; report unrun and failed checks accurately.
- New dependencies must use pinned release commits and have documented licenses.
- Do not commit `.tools`, build products, credentials, or downloaded dependency archives.
- DedicatedServer defaults to loopback. Public listening requires an explicit option.
