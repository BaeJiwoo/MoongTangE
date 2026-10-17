# External dependencies

No external physics engine is used. Common and ServerCore do not link raylib or Asio.

| Library | Pinned release / commit | License | Purpose |
| --- | --- | --- | --- |
| [raylib](https://github.com/raysan5/raylib/releases/tag/6.0) | 6.0 / `dbc56a87da87d973a9c5baa4e7438a9d20121d28` | zlib/libpng | Client rendering and input |
| [Standalone Asio](https://github.com/chriskohlhoff/asio/releases/tag/asio-1-38-2) | 1.38.2 / `8806a6803cde7054c3049d3666d3ec36786568c5` | Boost Software License 1.0 | TCP transport |

CMake verifies the downloaded source archive against its SHA-256. raylib's source archive includes its license and the notices for embedded dependencies (including GLFW). Preserve those notices when redistributing binaries and source.

Build tools: CMake >=3.25 and a C++20 compiler; Python 3 is required for network process tests when BUILD_TESTING is enabled. The client needs desktop OpenGL and platform windowing development libraries. On Debian/Ubuntu install `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.

Quality tools are development-only and are not linked into game binaries. clang-format and the locally checked clang-tidy are LLVM 22.1.3 (`llvmorg-22.1.3`, source commit `e9846648fd6183ee6d8cbdb4502213fcf902a211`), licensed under Apache-2.0 WITH LLVM-exception. CI installs the pinned [clang-format 22.1.3 Python distribution](https://pypi.org/project/clang-format/22.1.3/), whose packaging is Apache-2.0. See the [LLVM license](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.3/LICENSE.TXT) and [wheel project](https://github.com/ssciwr/clang-format-wheel). No formatter or analyzer binary is committed to this repository.
