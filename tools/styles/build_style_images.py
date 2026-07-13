#!/usr/bin/env python3
"""Draws the SVG images of the Dark and Light interface styles into data/styles/<Style>/.

--check only compares with the files on disk and exits with 1 when they differ.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STYLES = ROOT / "data" / "styles"

PALETTES = {
    "Dark": {
        "title_top": "#2a2a2a", "title_mid": "#161616", "title_low_top": "#060606", "title_low_bot": "#000000",
        "title_frame": "#5a5a5a", "title_inner": "#0c0c0c",
        "box_top": "#4c4c4c", "box_bot": "#383838", "box_line": "#151515", "box_hover_line": "#9a9a9a",
        "box_disabled": "#5a5a5a", "mark": "#e6e6e6", "mark_disabled": "#8a8a8a",
        "btn_fill": "#2a2a2a", "btn_line": "#161616", "btn_hover_fill": "#404040", "glyph": "#e0e0e0",
        "glyph_hover": "#ffffff", "glyph_disabled": "#6a6a6a",
        "close_fill": "#b4b4b4", "close_line": "#2a2a2a", "close_x": "#1a1a1a",
        "close_hover_fill": "#e8e8e8", "close_hover_x": "#c81e1e",
        "branch": "#9a9a9a", "branch_hover": "#dddddd", "grip": "#7a7a7a",
    },
    "Light": {
        "title_top": "#a8a8a8", "title_mid": "#9c9c9c", "title_low_top": "#929292", "title_low_bot": "#868686",
        "title_frame": "#7a7a7a", "title_inner": "#b4b4b4",
        "box_top": "#ffffff", "box_bot": "#e4e4e4", "box_line": "#8a8a8a", "box_hover_line": "#3a3a3a",
        "box_disabled": "#c4c4c4", "mark": "#222222", "mark_disabled": "#8a8a8a",
        "btn_fill": "#e2e2e2", "btn_line": "#a6a6a6", "btn_hover_fill": "#f6f6f6", "glyph": "#404040",
        "glyph_hover": "#000000", "glyph_disabled": "#a0a0a0",
        "close_fill": "#8c8c8c", "close_line": "#5c5c5c", "close_x": "#ffffff",
        "close_hover_fill": "#c81e1e", "close_hover_x": "#ffffff",
        "branch": "#555555", "branch_hover": "#000000", "grip": "#8a8a8a",
    },
}


def svg(width: int, height: int, body: str, defs: str = "") -> str:
    head = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
            f'viewBox="0 0 {width} {height}">\n')
    if defs:
        head += f"<defs>\n{defs}</defs>\n"
    return head + body + "</svg>\n"


def vgrad(gid: str, stops: list[tuple[float, str]]) -> str:
    s = "".join(f'<stop offset="{o:g}" stop-color="{c}"/>' for o, c in stops)
    return f'<linearGradient id="{gid}" x1="0" y1="0" x2="0" y2="1">{s}</linearGradient>\n'


# Twice the size of the 300x40 strip. The style sheet stretches it with a 4 px border slice.
def panel_title(p: dict) -> str:
    defs = vgrad("top", [(0, p["title_top"]), (1, p["title_mid"])])
    defs += vgrad("low", [(0, p["title_low_top"]), (1, p["title_low_bot"])])
    body = (
        f'<rect x="0" y="0" width="600" height="80" fill="{p["title_frame"]}"/>\n'
        f'<rect x="2" y="2" width="596" height="76" fill="{p["title_inner"]}"/>\n'
        '<rect x="4" y="4" width="592" height="72" fill="url(#top)"/>\n'
        '<path d="M4 46 C150 24 450 24 596 46 L596 76 L4 76 Z" fill="url(#low)"/>\n'
    )
    return svg(600, 80, body, defs)


def check_box(p: dict, checked: bool, hover: bool, disabled: bool = False, partial: bool = False) -> str:
    defs = vgrad("f", [(0, p["box_top"]), (1, p["box_bot"])])
    line = p["box_hover_line"] if hover else p["box_line"]
    fill = p["box_disabled"] if disabled else "url(#f)"
    mark = p["mark_disabled"] if disabled else p["mark"]
    body = f'<rect x="1.5" y="1.5" width="13" height="13" rx="2.5" fill="{fill}" stroke="{line}"/>\n'
    if checked:
        body += (f'<path d="M4.2 8.3 L6.9 11 L11.8 5.2" fill="none" stroke="{mark}" stroke-width="2" '
                 'stroke-linecap="round" stroke-linejoin="round"/>\n')
    elif partial:
        body += f'<rect x="4.5" y="7" width="7" height="2" rx="1" fill="{mark}"/>\n'
    return svg(16, 16, body, defs)


def radio(p: dict, checked: bool, hover: bool, disabled: bool = False) -> str:
    defs = vgrad("f", [(0, p["box_top"]), (1, p["box_bot"])])
    line = p["box_hover_line"] if hover else p["box_line"]
    fill = p["box_disabled"] if disabled else "url(#f)"
    mark = p["mark_disabled"] if disabled else p["mark"]
    body = f'<circle cx="8" cy="8" r="6.5" fill="{fill}" stroke="{line}"/>\n'
    if checked:
        body += f'<circle cx="8" cy="8" r="3" fill="{mark}"/>\n'
    return svg(16, 16, body, defs)


# Arrow buttons for scroll bars and combo boxes.
CHEVRONS = {
    "up": "M5 9.5 L8 6.5 L11 9.5",
    "down": "M5 6.5 L8 9.5 L11 6.5",
    "left": "M9.5 5 L6.5 8 L9.5 11",
    "right": "M6.5 5 L9.5 8 L6.5 11",
}


def arrow_button(p: dict, direction: str, state: str) -> str:
    fill = p["btn_hover_fill"] if state == "hover" else p["btn_fill"]
    glyph = {"normal": p["glyph"], "hover": p["glyph_hover"], "disabled": p["glyph_disabled"]}[state]
    body = (f'<circle cx="8" cy="8" r="6.5" fill="{fill}" stroke="{p["btn_line"]}"/>\n'
            f'<path d="{CHEVRONS[direction]}" fill="none" stroke="{glyph}" stroke-width="1.8" '
            'stroke-linecap="round" stroke-linejoin="round"/>\n')
    return svg(16, 16, body)


# Spin box triangles.
TRIANGLES = {"up": "M1 6 L4 2 L7 6 Z", "down": "M1 2 L4 6 L7 2 Z"}


def triangle(p: dict, direction: str, state: str) -> str:
    glyph = {"normal": p["glyph"], "hover": p["glyph_hover"], "disabled": p["glyph_disabled"]}[state]
    return svg(8, 8, f'<path d="{TRIANGLES[direction]}" fill="{glyph}"/>\n')


def tab_close(p: dict, hover: bool) -> str:
    fill = p["close_hover_fill"] if hover else p["close_fill"]
    x = p["close_hover_x"] if hover else p["close_x"]
    body = (f'<rect x="1.5" y="1.5" width="13" height="13" rx="3" fill="{fill}" stroke="{p["close_line"]}"/>\n'
            f'<path d="M5.2 5.2 L10.8 10.8 M10.8 5.2 L5.2 10.8" stroke="{x}" stroke-width="2" '
            'stroke-linecap="round"/>\n')
    return svg(16, 16, body)


def branch(p: dict, is_open: bool, hover: bool = False) -> str:
    c = p["branch_hover"] if hover else p["branch"]
    d = "M3.5 5.5 L12.5 5.5 L8 11.5 Z" if is_open else "M5.5 3.5 L11.5 8 L5.5 12.5 Z"
    return svg(16, 16, f'<path d="{d}" fill="{c}"/>\n')


def grip(p: dict, vertical: bool) -> str:
    dots = []
    for i in range(6):
        if vertical:
            dots.append(f'<rect x="{3 + i * 4}" y="2" width="2" height="2" fill="{p["grip"]}"/>')
        else:
            dots.append(f'<rect x="2" y="{3 + i * 4}" width="2" height="2" fill="{p["grip"]}"/>')
    w, h = (28, 6) if vertical else (6, 28)
    return svg(w, h, "\n".join(dots) + "\n")


def images_for(p: dict) -> dict[str, str]:
    out = {"panel/title.svg": panel_title(p)}
    for checked in (False, True):
        base = "checked" if checked else "unchecked"
        out[f"checkbox/{base}.svg"] = check_box(p, checked, False)
        out[f"checkbox/{base}_hover.svg"] = check_box(p, checked, True)
        out[f"checkbox/{base}_disabled.svg"] = check_box(p, checked, False, disabled=True)
        out[f"radiobutton/{base}.svg"] = radio(p, checked, False)
        out[f"radiobutton/{base}_hover.svg"] = radio(p, checked, True)
        out[f"radiobutton/{base}_disabled.svg"] = radio(p, checked, False, disabled=True)
    out["checkbox/indeterminate.svg"] = check_box(p, False, False, partial=True)
    out["checkbox/indeterminate_hover.svg"] = check_box(p, False, True, partial=True)
    for d in CHEVRONS:
        out[f"arrows/{d}.svg"] = arrow_button(p, d, "normal")
        out[f"arrows/{d}_hover.svg"] = arrow_button(p, d, "hover")
        out[f"arrows/{d}_disabled.svg"] = arrow_button(p, d, "disabled")
    for d in TRIANGLES:
        out[f"spinbox/{d}.svg"] = triangle(p, d, "normal")
        out[f"spinbox/{d}_hover.svg"] = triangle(p, d, "hover")
        out[f"spinbox/{d}_disabled.svg"] = triangle(p, d, "disabled")
    out["tabs/close.svg"] = tab_close(p, False)
    out["tabs/close_hover.svg"] = tab_close(p, True)
    out["treeview/branch-closed.svg"] = branch(p, False)
    out["treeview/branch-open.svg"] = branch(p, True)
    out["treeview/branch-closed_hover.svg"] = branch(p, False, True)
    out["treeview/branch-open_hover.svg"] = branch(p, True, True)
    out["toolbar/handle.svg"] = grip(p, False)
    out["toolbar/handle_vertical.svg"] = grip(p, True)
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="only compare with the files on disk")
    args = parser.parse_args()
    stale = []
    for style, palette in PALETTES.items():
        for rel, text in images_for(palette).items():
            path = STYLES / style / rel
            data = text.encode("utf-8")
            if path.is_file() and path.read_bytes() == data:
                continue
            stale.append(path)
            if not args.check:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
    if args.check:
        for path in stale:
            print(f"out of date: {path.relative_to(ROOT)}")
        return 1 if stale else 0
    print(f"{len(stale)} file(s) written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
