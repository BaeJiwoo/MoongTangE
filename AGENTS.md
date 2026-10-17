# Project development rules

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
- Format project C++ with clang-format 22.1.3 and run `Tools/check_format.py`. Keep headers self-contained; run the selected `.clang-tidy` checks for relevant C++ changes.
- New dependencies must use pinned release commits and have documented licenses.
- Do not commit `.tools`, build products, credentials, or downloaded dependency archives.
- DedicatedServer defaults to loopback. Public listening requires an explicit option.
