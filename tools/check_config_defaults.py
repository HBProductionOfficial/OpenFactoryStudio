#!/usr/bin/env python3
"""Checks that the settings fallbacks in the code, as in intValue(key, 6), match data/config.xml."""

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# These fallbacks differ on purpose: 0 means no window geometry is stored yet.
EXCEPTIONS = {"window_width", "window_height", "window_left", "window_top"}

CALL = re.compile(
    r'\b(?:config\(\)|config|m_config|cfg)\s*(?:\.|->)\s*(intValue|boolValue|doubleValue|value)\(\s*'
    r'QStringLiteral\("([a-z_0-9]+)"\)\s*,\s*([^;]+?)\)\s*[;),?:]'
)


def literal(text):
    text = text.strip()
    m = re.fullmatch(r'QStringLiteral\("(.*)"\)', text)
    if m:
        return m.group(1)
    if text in ("true", "false") or re.fullmatch(r"-?\d+(\.\d+)?", text):
        return text
    return None


def agrees(kind, fallback, default):
    if kind == "boolValue":
        return (fallback == "true") == (default.lower() in ("1", "true"))
    if kind in ("intValue", "doubleValue"):
        try:
            return float(fallback) == float(default)
        except ValueError:
            return True
    return fallback == default


def main():
    defaults = {e.get("name"): e.get("value", "") for e in ET.parse(ROOT / "data" / "config.xml").getroot()}
    problems = []
    for path in sorted((ROOT / "src").rglob("*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for m in CALL.finditer(text):
            kind, key, raw = m.groups()
            value = literal(raw)
            if value is None or key in EXCEPTIONS or key not in defaults:
                continue
            if not agrees(kind, value, defaults[key]):
                line = text.count("\n", 0, m.start()) + 1
                problems.append(f"{path.relative_to(ROOT)}:{line}: {key}: fallback {value!r}, "
                                f"data/config.xml {defaults[key]!r}")
    for problem in problems:
        print(problem)
    if problems:
        print(f"{len(problems)} fallback(s) disagree with data/config.xml")
        return 1
    print("Settings fallbacks agree with data/config.xml")
    return 0


if __name__ == "__main__":
    sys.exit(main())
