"""Checks that every vendored library in third_party/ ships a license file."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    third_party = ROOT / "third_party"
    problems = []
    if third_party.is_dir():
        for lib in sorted(p for p in third_party.iterdir() if p.is_dir()):
            if not any(f.name.upper().startswith(("LICENSE", "COPYING", "UNLICENSE")) for f in lib.iterdir()):
                problems.append(f"third_party/{lib.name}: no LICENSE/COPYING file")
    for p in problems:
        print(p)
    if not problems:
        print("third-party licenses OK")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
