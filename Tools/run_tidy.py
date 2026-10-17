"""Run the selected clang-tidy checks over every project C++ translation unit.

Requires a configured full build so pinned Asio and raylib headers are available.
This lightweight driver does not lint third-party or generated source files.
"""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang-tidy", default="clang-tidy")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    options = parser.parse_args()
    dependencies = options.build_dir.resolve() / "_deps"
    includes = (dependencies / "asio-src/include", dependencies / "raylib-src/src")
    if not all(path.is_dir() for path in includes):
        parser.error("Configure a full build with Asio and raylib before running analysis.")
    flags = ["-std=c++20", "-I" + str(ROOT), "-DASIO_STANDALONE", "-DSANDBOX_HAS_NETWORK"]
    for path in includes:
        flags.extend(("-isystem", str(path)))
    if os.name == "nt":
        flags.extend(("-D_WIN32_WINNT=0x0A00", "-D_CRT_SECURE_NO_WARNINGS"))
    else:
        flags.append("-pthread")
    sources = sorted(path for directory in ("Client", "Server", "Tests")
                     for path in (ROOT / directory).rglob("*.cpp"))
    subprocess.run([options.clang_tidy, *map(str, sources), "--", *flags], cwd=ROOT, check=True)
    print(f"Analyzed {len(sources)} translation units with .clang-tidy checks.")


if __name__ == "__main__":
    main()
