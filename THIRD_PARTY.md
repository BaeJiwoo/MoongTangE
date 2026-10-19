# External dependencies

No external physics engine is used. Common and ServerCore do not link raylib, Asio or Protobuf.

| Library | Pinned release / commit | License | Purpose |
| --- | --- | --- | --- |
| [raylib](https://github.com/raysan5/raylib/releases/tag/6.0) | 6.0 / `dbc56a87da87d973a9c5baa4e7438a9d20121d28` | zlib/libpng | Client rendering and input |
| [Standalone Asio](https://github.com/chriskohlhoff/asio/releases/tag/asio-1-38-2) | 1.38.2 / `8806a6803cde7054c3049d3666d3ec36786568c5` | Boost Software License 1.0 | TCP transport |
| [Protobuf](https://github.com/protocolbuffers/protobuf/releases/tag/v36.2) | protoc 36.2 / C++ 7.36.2 / `2c74169b34066ceb8ddb6b882fcb3fb32d737a55` | BSD-3-Clause | Network message generation and serialization |
| [Abseil](https://github.com/abseil/abseil-cpp/releases/tag/20250512.1) | 20250512.1 / `76bb24329e8bf5f39704eb10d21b9a80befa7c81` | Apache-2.0 | Protobuf's pinned C++ dependency |
| utf8_range | Included in the pinned Protobuf archive | MIT | Protobuf UTF-8 support |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib/releases/tag/v0.61.0) | 0.61.0 / `c419c904da4ed0d057f169c225c58cc795c07e4d` | MIT | HTTP matchmaking control API |
| upb | Included in the pinned Protobuf archive | BSD-3-Clause (Protobuf license) | Protobuf compiler support |

CMake verifies the downloaded source archive against its SHA-256. raylib's source archive includes its license and the notices for embedded dependencies (including GLFW). Preserve those notices when redistributing binaries and source.

cpp-httplib is fetched only in network builds through the private `MatchmakingHttp` dependency of `ServerRuntime`. Its optional TLS/compression libraries and examples/tests are not enabled. Preserve its [LICENSE](https://github.com/yhirose/cpp-httplib/blob/c419c904da4ed0d057f169c225c58cc795c07e4d/LICENSE) when redistributing. Common and ServerCore remain independent of HTTP and OS libraries.

`cmake/Protobuf.cmake` builds protoc and the runtime from exactly the same pinned source. Abseil uses the version selected by that Protobuf release. System Protobuf/Abseil installations are not substituted. Protobuf's bundled upb and utf8_range sources need no separate download. Preserve `protobuf/LICENSE`, `protobuf/third_party/utf8_range/LICENSE`, and `absl/LICENSE` with redistribution. zlib, conformance tests and dependency examples are disabled. Generated game messages stay in the build directory and are regenerated from the repository's `.proto` schema.

Protobuf, Abseil, utf8_range and upb are downloaded only when `MOONGTANGE_BUILD_NETWORK=ON`. Neither Common nor ServerCore links them; a core-only configuration downloads no external source. A clean full build takes longer because it also compiles protoc and its runtime dependencies.

Build tools: CMake >=3.25 and a C++20 compiler; Python 3 is required for network process tests when BUILD_TESTING is enabled. The client needs desktop OpenGL and platform windowing development libraries. On Debian/Ubuntu install `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.

Quality tools are development-only and are not linked into game binaries. clang-format and the locally checked clang-tidy are LLVM 22.1.3 (`llvmorg-22.1.3`, source commit `e9846648fd6183ee6d8cbdb4502213fcf902a211`), licensed under Apache-2.0 WITH LLVM-exception. CI installs the pinned [clang-format 22.1.3 Python distribution](https://pypi.org/project/clang-format/22.1.3/), whose packaging is Apache-2.0. See the [LLVM license](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.3/LICENSE.TXT) and [wheel project](https://github.com/ssciwr/clang-format-wheel). No formatter or analyzer binary is committed to this repository.
