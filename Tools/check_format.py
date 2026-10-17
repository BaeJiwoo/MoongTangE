"""Check or apply the project's pinned C++ formatting; never scan downloaded dependencies."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
VERSION = "22.1.3"
SOURCE_DIRECTORIES = ("Client", "Common", "Network", "Server", "Tests")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang-format", default="clang-format")
    parser.add_argument("--fix", action="store_true")
    options = parser.parse_args()
    version = subprocess.check_output([options.clang_format, "--version"], text=True)
    if not re.search(r"\b" + re.escape(VERSION) + r"\b", version):
        parser.error(f"Use clang-format {VERSION}; found {version.strip()}")
    sources = sorted(path for directory in SOURCE_DIRECTORIES
                     for path in (ROOT / directory).rglob("*")
                     if path.suffix in (".h", ".cpp"))
    arguments = ["-i"] if options.fix else ["--dry-run", "--Werror"]
    subprocess.run([options.clang_format, *arguments, *map(str, sources)], cwd=ROOT, check=True)
    print(f"{'Formatted' if options.fix else 'Verified'} {len(sources)} C++ files ({VERSION}).")


if __name__ == "__main__":
    main()
