"""Prints the CHANGELOG.md entry of a version, for the notes of its GitHub release.

Usage: python tools/release_notes.py 1.2.0
"""
import sys
from pathlib import Path

CHANGELOG = Path(__file__).resolve().parent.parent / "CHANGELOG.md"


def entry(text, version):
    lines = text.splitlines()
    heading = f"## Version {version}"
    start = next((i for i, line in enumerate(lines) if line == heading or line.startswith(heading + " ")), None)
    if start is None:
        return ""
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("## ")), len(lines))
    return "\n".join(lines[start + 1 : end]).strip()


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: release_notes.py <version>")
    version = sys.argv[1].removeprefix("v")
    notes = entry(CHANGELOG.read_text(encoding="utf-8"), version)
    if not notes:
        sys.exit(f"CHANGELOG.md has no entry for version {version}")
    print(notes)


if __name__ == "__main__":
    main()
