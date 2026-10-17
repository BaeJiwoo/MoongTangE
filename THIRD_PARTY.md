# External dependencies

No external physics engine is used. Common and ServerCore do not link raylib or Asio.

| Library | Pinned release / commit | License | Purpose |
| --- | --- | --- | --- |
| [raylib](https://github.com/raysan5/raylib/releases/tag/6.0) | 6.0 / `dbc56a87da87d973a9c5baa4e7438a9d20121d28` | zlib/libpng | Client rendering and input |

CMake verifies the downloaded source archive against its SHA-256. raylib's source archive includes its license and the notices for embedded dependencies (including GLFW). Preserve those notices when redistributing binaries and source.

Build tools: CMake >=3.25 and a C++20 compiler. The client needs desktop OpenGL and platform windowing development libraries. On Debian/Ubuntu install `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.
