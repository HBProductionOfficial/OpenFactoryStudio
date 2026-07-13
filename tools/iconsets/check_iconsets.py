#!/usr/bin/env python3
"""Checks the shipped icon sets: required names, readable files, the Sources files and license files."""
from __future__ import annotations

import argparse
import json
import os
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REQUIRED_SETS = ("Nuvola", "Flat-Dark", "Flat-Light")


def parse_iconset(folder: Path) -> list[tuple[str, str]]:
    root = ET.parse(folder / "iconset.xml").getroot()
    if root.tag != "iconset":
        raise ValueError(f"{folder}/iconset.xml: root element is <{root.tag}>")
    return [(e.get("name", ""), e.get("file", "")) for e in root.iter("icon")]


def resolve(file: str, folder: Path, global_folder: Path | None) -> Path:
    if global_folder is not None and file.startswith("*"):
        return Path(os.path.normpath(global_folder / file[1:]))
    return Path(os.path.normpath(folder / file))


def check_image(path: Path) -> str | None:
    data = path.read_bytes()
    if path.suffix.lower() == ".png":
        if data[:8] != b"\x89PNG\r\n\x1a\n":
            return "not a PNG file"
        w, h = struct.unpack(">II", data[16:24])
        if w == 0 or h == 0:
            return "empty PNG"
        return None
    if path.suffix.lower() == ".svg":
        try:
            root = ET.fromstring(data)
        except ET.ParseError as e:
            return f"invalid SVG: {e}"
        if not root.tag.endswith("svg"):
            return "SVG root element is not <svg>"
        return None
    return f"unsupported image type {path.suffix}"


def check_folder_docs(folder: Path) -> list[str]:
    problems = []
    if not (folder / "Sources.pdf").is_file():
        problems.append(f"{folder}: no Sources.pdf")
    if not any(p.name.upper().startswith(("LICENSE", "COPYING")) for p in folder.iterdir()):
        problems.append(f"{folder}: no license file")
    return problems


def check_set(name: str, data: Path, required: dict, complete: bool) -> tuple[list[str], str]:
    problems: list[str] = []
    gdir = data / "iconsets" / name
    edir = data / "engines" / "mugen" / "iconsets" / name
    entries_g = parse_iconset(gdir)
    entries_e = parse_iconset(edir) if (edir / "iconset.xml").is_file() else []
    problems += check_folder_docs(gdir)
    if entries_e:
        problems += check_folder_docs(edir)

    seen_files: dict[Path, str | None] = {}
    for entries, folder, glob in ((entries_g, gdir, None), (entries_e, edir, gdir)):
        names = set()
        for icon, file in entries:
            if not icon or not file:
                problems.append(f"{folder}/iconset.xml: entry without name or file")
                continue
            if icon in names:
                problems.append(f"{folder}/iconset.xml: {icon} defined twice")
            names.add(icon)
            path = resolve(file, folder, glob)
            if path not in seen_files:
                seen_files[path] = check_image(path) if path.is_file() else "missing file"
            if seen_files[path]:
                problems.append(f"{name}: {icon} -> {file}: {seen_files[path]}")

    gnames = {n for n, _ in entries_g}
    enames = {n for n, _ in entries_e}
    miss_g = sorted(set(required["global"]) - gnames)
    miss_e = sorted(set(required["mugen"]) - enames)
    if complete:
        for n in miss_g:
            problems.append(f"{name}: global name without an icon: {n}")
        for n in miss_e:
            problems.append(f"{name}: M.U.G.E.N name without an icon: {n}")
    have_g = len(required["global"]) - len(miss_g)
    have_e = len(required["mugen"]) - len(miss_e)
    summary = (f"{name:<11} global {have_g}/{len(required['global'])}  mugen {have_e}/{len(required['mugen'])}  "
               f"files {len(seen_files)}")
    return problems, summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--data", type=Path, default=ROOT / "data", help="data folder (default: <repo>/data)")
    parser.add_argument("--all", action="store_true", help="require every set to be complete")
    args = parser.parse_args()
    required = json.loads((Path(__file__).resolve().parent / "required_names.json").read_text(encoding="utf-8"))
    sets = sorted(p.name for p in (args.data / "iconsets").iterdir() if (p / "iconset.xml").is_file())
    problems = []
    for name in REQUIRED_SETS:
        if name not in sets:
            problems.append(f"required icon set missing: {name}")
    for name in sets:
        p, summary = check_set(name, args.data, required, args.all or name in REQUIRED_SETS)
        print(summary)
        problems += p
    for p in problems:
        print(p)
    print("icon sets OK" if not problems else f"{len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
