"""Checks that every third_party/ library and every icon set comes with its license files."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def has_license(folder: Path) -> bool:
    return any(f.name.upper().startswith(("LICENSE", "COPYING", "UNLICENSE")) for f in folder.iterdir())


def check_libraries() -> list[str]:
    problems = []
    third_party = ROOT / "third_party"
    if third_party.is_dir():
        for lib in sorted(p for p in third_party.iterdir() if p.is_dir()):
            if not has_license(lib):
                problems.append(f"third_party/{lib.name}: no LICENSE/COPYING file")
    return problems


def iconset_folders() -> list[Path]:
    data = ROOT / "data"
    roots = [data / "iconsets"] + sorted((data / "engines").glob("*/iconsets"))
    folders = []
    for root in roots:
        if root.is_dir():
            folders.extend(sorted(p for p in root.iterdir() if (p / "iconset.xml").is_file()))
    return folders


def check_iconsets() -> list[str]:
    problems = []
    for folder in iconset_folders():
        rel = folder.relative_to(ROOT).as_posix()
        if not (folder / "Sources.pdf").is_file():
            problems.append(f"{rel}: no Sources.pdf")
        if not has_license(folder):
            problems.append(f"{rel}: no license file")
    return problems


def main() -> int:
    problems = check_libraries() + check_iconsets()
    for p in problems:
        print(p)
    if not problems:
        print("third-party licenses OK")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
