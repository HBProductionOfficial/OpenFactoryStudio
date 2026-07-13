"""Our own drawings (GPL-3.0) for icons with no stock picture: g_* glossy 48x48, f_* flat 24x24.
Letters and digits use the outline glyphs of Tabler Icons (MIT)."""
from __future__ import annotations

import math

from art import (Svg, ball, bold_arrow, circle, clsn_box, curved_arrow, glossy_text, icon_glyph, line, outline_group,
                 path, poly, rect, red_x, sheet, sparkle, straight_arrow, text_elements)


# === Glossy drawings (48 x 48) ==================================================================
def g_ok(svg: Svg, ctx) -> None:
    ball(svg, 24, 24, 20, "green")
    svg.add(path("M13 24.5l7.5 7.5l14 -15", fill="none", stroke="#0d4a0b", stroke_width=7.5,
                 stroke_linecap="round", stroke_linejoin="round"))
    svg.add(path("M13 24.5l7.5 7.5l14 -15", fill="none", stroke="#ffffff", stroke_width=4.5,
                 stroke_linecap="round", stroke_linejoin="round"))


def g_warning(svg: Svg, ctx) -> None:
    fill = svg.linear([(0, "#fff7a0"), (0.5, "#ffd21a"), (1, "#e09a00")], 0, 0, 0, 1)
    d = "M24 4.5c1.6 0 2.6 0.9 3.4 2.3l17 30.2c1.5 2.7 0.2 5.5 -3 5.5h-34.8c-3.2 0 -4.5 -2.8 -3 -5.5l17 -30.2c0.8 -1.4 1.8 -2.3 3.4 -2.3z"
    svg.add(path(d, fill=fill, stroke="#8a5a00", stroke_width=1.6, stroke_linejoin="round"))
    svg.add(path("M24 15v13", fill="none", stroke="#222222", stroke_width=5.2, stroke_linecap="round"))
    svg.add(circle(24, 35.5, 3.1, fill="#222222"))


def g_add(svg: Svg, ctx) -> None:
    sparkle(svg, 24, 24, 21)


def g_add_all(svg: Svg, ctx) -> None:
    sparkle(svg, 16, 16, 14)
    sparkle(svg, 32, 32, 15)


def g_crosshair(svg: Svg, ctx) -> None:
    for w, c in ((5.0, "#ffffff"), (2.4, "#202020")):
        svg.add(path("M24 6v36M6 24h36", fill="none", stroke=c, stroke_width=w, stroke_linecap="round"))


def g_red_x(svg: Svg, ctx) -> None:
    red_x(svg, 24, 24, 18)


def g_check(svg: Svg, ctx) -> None:
    fill = svg.linear([(0, "#a8f070"), (1, "#2a9a10")], 0, 0, 0, 1)
    d = "M5 25l7 -7l8 8l16 -18l7 7l-23 25z"
    svg.add(path(d, fill=fill, stroke="#145a06", stroke_width=1.6, stroke_linejoin="round"))


# --- collision boxes ------------------------------------------------------------------------------
def g_clsn1_add(svg: Svg, ctx) -> None:
    clsn_box(svg, 13, 13, 30, 30, "attack")
    sparkle(svg, 13, 13, 12)


def g_clsn2_add(svg: Svg, ctx) -> None:
    clsn_box(svg, 13, 13, 30, 30, "collision")
    sparkle(svg, 13, 13, 12)


def _box_pair(svg: Svg, ox: float = 0, oy: float = 0, s: float = 1.0) -> None:
    clsn_box(svg, ox + 6 * s, oy + 6 * s, 24 * s, 24 * s, "attack", 3 * s)
    clsn_box(svg, ox + 17 * s, oy + 17 * s, 24 * s, 24 * s, "collision", 3 * s)


def g_clsn_delete(svg: Svg, ctx) -> None:
    _box_pair(svg)
    red_x(svg, 15, 15, 11)


def g_clsn_delete_frame(svg: Svg, ctx) -> None:
    _box_pair(svg, 8, 6, 0.8)
    _box_pair(svg, 0, 0, 0.8)
    red_x(svg, 13, 13, 10)
    red_x(svg, 31, 13, 8)


def g_clsn_delete_anim(svg: Svg, ctx) -> None:
    # a film strip behind the boxes
    svg.add(rect(3, 10, 42, 30, rx=2, fill="#3a3a3a", stroke="#111111", stroke_width=1.2))
    for i in range(7):
        svg.add(rect(5 + i * 6, 12, 3, 3, fill="#e8e8e8"))
        svg.add(rect(5 + i * 6, 35, 3, 3, fill="#e8e8e8"))
    svg.add(rect(6, 17, 36, 16, fill="#a8c8f0"))
    clsn_box(svg, 9, 19, 14, 12, "attack", 2.2)
    clsn_box(svg, 25, 19, 14, 12, "collision", 2.2)
    red_x(svg, 34, 34, 11)


def g_clsn_previous(svg: Svg, ctx) -> None:
    clsn_box(svg, 18, 18, 26, 26, "attack")
    clsn_box(svg, 24, 24, 18, 18, "collision", 2.6)
    curved_arrow(svg, [(30, 12), (14, 2), (6, 20)], "blue", 6, 7.5)


def g_clsn_switch(svg: Svg, ctx) -> None:
    clsn_box(svg, 4, 4, 18, 18, "attack", 2.6)
    clsn_box(svg, 26, 26, 18, 18, "collision", 2.6)
    curved_arrow(svg, [(28, 8), (40, 8), (40, 20)], "grey", 4.5, 6)
    curved_arrow(svg, [(20, 40), (8, 40), (8, 28)], "grey", 4.5, 6)


def g_clsn_default(svg: Svg, ctx) -> None:
    svg.add(rect(5, 5, 26, 26, fill="#d8000020", stroke="#d80000", stroke_width=2.6, stroke_dasharray="4 2.5"))
    svg.add(rect(17, 17, 26, 26, fill="#0016d820", stroke="#0016d8", stroke_width=2.6, stroke_dasharray="4 2.5"))
    fill = svg.linear([(0, "#4a8cff"), (1, "#0a2a9a")], 0, 0, 0, 1)
    svg.add(rect(9, 17, 30, 14, rx=2, fill=fill, stroke="#061a60", stroke_width=1.2))
    els, _ = text_elements(ctx.tabler, "DEF", 24, 28, 8.5, "#ffffff", 2.0, anchor="middle")
    svg.body.extend(els)


def g_view_box(kind: str):
    def draw(svg: Svg, ctx) -> None:
        clsn_box(svg, 5, 5, 26, 26, kind, 3.2)
        clsn_box(svg, 17, 17, 26, 26, kind, 3.2)
    return draw


# --- "use in all next frames" ----------------------------------------------------------------------
def _all_frames(svg: Svg, glyph) -> None:
    sheet(svg, 6, 3, 30, 38, fold=8)
    glyph(svg)
    for dx in (0, 7):
        bold_arrow(svg, f"M{29 + dx} 30l9 7.5l-9 7.5l-3.2 -2.6l5.8 -4.9l-5.8 -4.9z", "blue", 1.0)


def g_all_time(svg: Svg, ctx) -> None:
    def glyph(s):
        s.add(circle(20, 22, 10, fill="#ffffff", stroke="#2a62c8", stroke_width=2.6))
        s.add(path("M20 15v7.5l5 3", fill="none", stroke="#222222", stroke_width=2.4, stroke_linecap="round",
                   stroke_linejoin="round"))
    _all_frames(svg, glyph)


def g_all_flip(svg: Svg, ctx) -> None:
    def glyph(s):
        s.add(path("M20 11v24", stroke="#555555", stroke_width=1.6, stroke_dasharray="3 2"))
        s.add(poly([(17, 13), (17, 31), (9, 31)], fill="#2a62c8", stroke="#0b2f7a", stroke_width=1.2))
        s.add(poly([(23, 13), (23, 31), (31, 31)], fill="#ffffff", stroke="#0b2f7a", stroke_width=1.2))
    _all_frames(svg, glyph)


def g_all_offset(svg: Svg, ctx) -> None:
    def glyph(s):
        d = ("M20 9l4 4h-2.5v6.5h6.5v-2.5l4 4l-4 4v-2.5h-6.5v6.5h2.5l-4 4l-4 -4h2.5v-6.5h-6.5v2.5l-4 -4l4 -4v2.5"
             "h6.5v-6.5h-2.5z")
        s.add(path(d, fill="#2a62c8", stroke="#0b2f7a", stroke_width=1.0, stroke_linejoin="round"))
    _all_frames(svg, glyph)


def g_all_scale(svg: Svg, ctx) -> None:
    def glyph(s):
        s.add(rect(11, 15, 12, 12, fill="#ffffff", stroke="#555555", stroke_width=1.4))
        s.add(path("M16 22l14 -12M30 10h-6M30 10v6", fill="none", stroke="#2a62c8", stroke_width=2.6,
                   stroke_linecap="round", stroke_linejoin="round"))
    _all_frames(svg, glyph)


def g_all_trans(svg: Svg, ctx) -> None:
    def glyph(s):
        for i in range(4):
            for j in range(4):
                c = "#9a9a9a" if (i + j) % 2 else "#ffffff"
                s.add(rect(11 + i * 4.5, 12 + j * 4.5, 4.5, 4.5, fill=c))
        s.add(rect(11, 12, 18, 18, fill="#2a62c880", stroke="#0b2f7a", stroke_width=1.2))
    _all_frames(svg, glyph)


# --- code value types ------------------------------------------------------------------------------
def g_text(text: str, color: str, height: float = 20, italic: float = 10, width: float = 4.2,
           baseline: float = 34):
    def draw(svg: Svg, ctx) -> None:
        glossy_text(svg, ctx.tabler, text, 24, baseline, height, color, width, italic)
    return draw


def _darker(color: str, f: float = 0.55) -> str:
    r, g, b = int(color[1:3], 16), int(color[3:5], 16), int(color[5:7], 16)
    return "#%02x%02x%02x" % (int(r * f), int(g * f), int(b * f))


def g_icon_text(icon: str, color: str, target_w: float = 40, italic: float = 10, width: float = 4.0,
                cy: float = 25, quotes: bool = False):
    """A whole outline icon (abc, letter-case, regex ...) drawn as bold coloured lettering."""
    def draw(svg: Svg, ctx) -> None:
        svg.body.extend(icon_glyph(ctx.tabler, icon, 24, cy, target_w, color, width, italic,
                                   outline=_darker(color), outline_width=1.6, max_h=34))
        if quotes:
            for x, y in ((6, 12), (40, 42)):
                for dx in (-2.2, 2.2):
                    d = f"M{x + dx + 0.8} {y - 4.5}l-1.6 4.5"
                    svg.add(path(d, fill="none", stroke=_darker(color), stroke_width=3.6, stroke_linecap="round"))
                    svg.add(path(d, fill="none", stroke=color, stroke_width=2.2, stroke_linecap="round"))
    return draw


def g_date(svg: Svg, ctx) -> None:
    svg.add(circle(24, 24, 17, fill="#ffffff", stroke="#8a2ae6", stroke_width=4))
    svg.add(path("M24 13v11l7 5", fill="none", stroke="#8a2ae6", stroke_width=4, stroke_linecap="round",
                 stroke_linejoin="round"))


def g_bool(svg: Svg, ctx) -> None:
    svg.add(rect(5, 14, 38, 20, rx=10, fill="#ffffff", stroke="#16a58a", stroke_width=3.6))
    svg.add(circle(15, 24, 6, fill="#16a58a"))


def g_comment(svg: Svg, ctx) -> None:
    sheet(svg, 7, 3, 32, 40, fold=8, lines=0)
    glossy_text(svg, ctx.tabler, ";", 18, 32, 18, "#7a7a7a", 3.8, 8)
    for i, w in enumerate((10, 14, 8)):
        svg.add(line(22, 20 + i * 6, 22 + w, 20 + i * 6, stroke="#9aa6b8", stroke_width=2, stroke_linecap="round"))


def g_cmd_type(svg: Svg, ctx) -> None:
    ball(svg, 15, 16, 12, "yellow")
    ball(svg, 33, 32, 12, "green")
    for txt, cx, by in (("A", 15, 21), ("Z", 33, 37)):
        els, _ = text_elements(ctx.tabler, txt, cx, by, 10, "#1a1a1a", 2.6, anchor="middle")
        svg.body.extend(els)


def g_flags(svg: Svg, ctx) -> None:
    els, _ = text_elements(ctx.tabler, "A", 9, 31, 15, "#e0187a", 3.2, italic=8, anchor="middle",
                           outline="#6a0a38", outline_width=1.4)
    svg.body.extend(els)
    els, _ = text_elements(ctx.tabler, "B", 21, 31, 15, "#22a03a", 3.2, italic=8, anchor="middle",
                           outline="#0a4a18", outline_width=1.4)
    svg.body.extend(els)
    els, _ = text_elements(ctx.tabler, ",C", 36, 31, 15, "#2a62e6", 3.2, italic=8, anchor="middle",
                           outline="#0a2a6a", outline_width=1.4)
    svg.body.extend(els)


# --- debugger -----------------------------------------------------------------------------------------
def _dot(svg: Svg, cx: float, cy: float) -> None:
    ball(svg, cx, cy, 5, "grey")


def g_step_over(svg: Svg, ctx) -> None:
    curved_arrow(svg, [(5, 32), (20, 0), (42, 28)], "blue", 8, 10)
    _dot(svg, 24, 40)


def g_step_into(svg: Svg, ctx) -> None:
    curved_arrow(svg, [(6, 6), (26, 4), (24, 32)], "blue", 8, 10)
    _dot(svg, 24, 41)


def g_step_out(svg: Svg, ctx) -> None:
    curved_arrow(svg, [(16, 34), (16, 6), (42, 8)], "blue", 8, 10)
    _dot(svg, 16, 41)


def g_current_line(svg: Svg, ctx) -> None:
    straight_arrow(svg, 24, 24, 38, 0, "yellow", 13, 15)


# --- view ------------------------------------------------------------------------------------------------
def g_guide_lines(svg: Svg, ctx) -> None:
    for w, c in ((5, "#ffffff"), (2.6, "#1a6ae6")):
        svg.add(path("M4 30h40M18 4v40", fill="none", stroke=c, stroke_width=w, stroke_linecap="round"))
    svg.add(circle(18, 30, 3.6, fill="#ffffff", stroke="#0b3a8c", stroke_width=1.4))


def g_center_origin(svg: Svg, ctx) -> None:
    fill = svg.linear([(0, "#ffffff"), (1, "#d8e4f4")], 0, 0, 1, 1)
    svg.add(rect(5, 5, 38, 38, rx=3, fill=fill, stroke="#5a6a80", stroke_width=1.4))
    for d in ("M10 17v-7h7", "M31 10h7v7", "M38 31v7h-7", "M17 38h-7v-7"):
        svg.add(path(d, fill="none", stroke="#1a4aa8", stroke_width=2.6, stroke_linecap="round", stroke_linejoin="round"))
    svg.add(path("M24 15v18M15 24h18", fill="none", stroke="#0a2a6a", stroke_width=3.6, stroke_linecap="round"))


def g_align(pos: str):
    """Sprite alignment to the guide lines; `pos` is one of tl, tc, tr, cl, cc, cr, bl, bc, br."""
    def draw(svg: Svg, ctx) -> None:
        gx, gy = 24, 24
        w, h = 18, 22
        x = {"l": gx, "c": gx - w / 2, "r": gx - w}[pos[1]]
        y = {"t": gy, "c": gy - h / 2, "b": gy - h}[pos[0]]
        svg.add(path("M2 24h44M24 2v44", fill="none", stroke="#6a8ad0", stroke_width=1.6, stroke_dasharray="3 2"))
        fill = svg.linear([(0, "#ffffff"), (1, "#c8c8c8")], 0, 0, 1, 1)
        svg.add(rect(x + 1, y + 1, w, h, rx=1.5, fill="#00000030"))
        svg.add(rect(x, y, w, h, rx=1.5, fill=fill, stroke="#6a6a6a", stroke_width=1.2))
        for wdt, c in ((5.4, "#ffffff"), (2.8, "#0a1a8a")):
            svg.add(path(f"M{gx - 6} {gy}h12M{gx} {gy - 6}v12", fill="none", stroke=c, stroke_width=wdt,
                         stroke_linecap="round"))
    return draw


# --- state types -----------------------------------------------------------------------------------------
def g_state(letter: str, color: str):
    def draw(svg: Svg, ctx) -> None:
        ball(svg, 24, 24, 20, color)
        els, _ = text_elements(ctx.tabler, letter, 24, 32.5, 17, "#141414", 3.6, anchor="middle")
        svg.body.extend(els)
    return draw


def g_var(color: str, window: bool):
    def draw(svg: Svg, ctx) -> None:
        sheet(svg, 6, 3, 34, 41, fold=8 if not window else 0.01, lines=4 if not window else 0)
        if window:
            bar = svg.linear([(0, "#7aa8f0"), (1, "#1a4ab0")], 0, 0, 0, 1)
            svg.add(rect(6, 3, 34, 7, fill=bar))
            for i in range(3):
                svg.add(line(10, 15 + i * 4.5, 34, 15 + i * 4.5, stroke="#9aa6b8", stroke_width=1.6, stroke_linecap="round"))
        fill = svg.linear([(0, "#ffffff"), (1, "#e0e0e0")], 0, 0, 0, 1)
        svg.add(rect(5, 27, 38, 15, rx=2, fill=fill, stroke=color, stroke_width=2))
        els, _ = text_elements(ctx.tabler, "VAR", 24, 39, 9, color, 2.4, anchor="middle")
        svg.body.extend(els)
    return draw


# --- palette tools -----------------------------------------------------------------------------------------
def _grid_balls(svg: Svg, colors: list[list[str]], r: float = 5.6, x0: float = 8, y0: float = 8, step: float = 13) -> None:
    for j, row in enumerate(colors):
        for i, c in enumerate(row):
            if c:
                ball(svg, x0 + i * step, y0 + j * step, r, c)


def g_gradient(svg: Svg, ctx) -> None:
    _grid_balls(svg, [["yellow", "yellow", "orange", "orange"], ["yellow", "orange", "orange", "red"],
                      ["orange", "orange", "red", "red"], ["orange", "red", "red", "brown"]], 5.2, 6, 6, 12)


def g_grayscale(svg: Svg, ctx) -> None:
    _grid_balls(svg, [["grey"] * 3] * 3, 6.4, 9, 9, 15)


def g_invert(svg: Svg, ctx) -> None:
    _grid_balls(svg, [["red", "orange", "yellow"], ["cyan", "blue", "purple"], ["grey", "green", ""]], 6.0, 9, 9, 14)
    curved_arrow(svg, [(30, 44), (44, 44), (42, 30)], "grey", 4, 6)


def g_switch_colors(svg: Svg, ctx) -> None:
    ball(svg, 14, 14, 10, "red")
    ball(svg, 34, 34, 10, "orange")
    curved_arrow(svg, [(26, 6), (42, 6), (40, 22)], "grey", 4.2, 6)
    curved_arrow(svg, [(22, 42), (6, 42), (8, 26)], "grey", 4.2, 6)


def g_switch_count(svg: Svg, ctx) -> None:
    _grid_balls(svg, [["red", "red"], ["red", "red"]], 5.6, 9, 9, 12)
    _grid_balls(svg, [["red", "red", "red"], ["red", "red", "red"], ["red", "red", "red"]], 3.8, 27, 27, 7.6)
    curved_arrow(svg, [(30, 6), (42, 6), (42, 18)], "grey", 4, 6)


def g_set_color(svg: Svg, ctx) -> None:
    ball(svg, 30, 30, 14, "red")
    curved_arrow(svg, [(4, 20), (6, 6), (20, 8)], "grey", 4.6, 7)


def g_add_missing(svg: Svg, ctx) -> None:
    ball(svg, 28, 30, 14, "red")
    sparkle(svg, 14, 14, 12)


def g_replace_duplicates(svg: Svg, ctx) -> None:
    ball(svg, 17, 22, 12, "yellow")
    ball(svg, 31, 22, 12, "yellow")
    red_x(svg, 33, 34, 10)


def _dashed_box(svg: Svg, x, y, w, h) -> None:
    svg.add(rect(x, y, w, h, fill="#ffffff80", stroke="#ffffff", stroke_width=2.4))
    svg.add(rect(x, y, w, h, fill="none", stroke="#202020", stroke_width=2.4, stroke_dasharray="4 3"))


def g_selection_to(direction: str):
    def draw(svg: Svg, ctx) -> None:
        if direction == "left":
            _dashed_box(svg, 4, 12, 20, 24)
            straight_arrow(svg, 34, 24, 22, 180, "blue", 8, 9)
        else:
            _dashed_box(svg, 24, 12, 20, 24)
            straight_arrow(svg, 14, 24, 22, 0, "blue", 8, 9)
    return draw


def g_colors_to(direction: str):
    def draw(svg: Svg, ctx) -> None:
        bx = 8 if direction == "left" else 30
        for i, c in enumerate(("red", "yellow", "orange")):
            ball(svg, bx + (i % 2) * 9, 12 + i * 11, 6.5, c)
        straight_arrow(svg, 34 if direction == "left" else 14, 24, 20, 180 if direction == "left" else 0, "blue", 8, 9)
    return draw


# --- image editor -----------------------------------------------------------------------------------------
def g_outlined(icon: str, fill: str = "#ffffff", edge: str = "#202020", accent=None):
    """An outline icon drawn as white strokes with a dark rim (gloves, cursors, arrows)."""
    def draw(svg: Svg, ctx) -> None:
        lib = ctx.lib_for(icon)
        name = icon.split(":")[-1]
        t = "translate(2,2) scale(1.8333)"
        svg.add(outline_group(lib, name, edge, 3.6, t))
        svg.add(outline_group(lib, name, fill, 1.7, t))
        if accent:
            accent(svg, ctx)
    return draw


def g_rect_select(svg: Svg, ctx) -> None:
    _dashed_box(svg, 7, 7, 34, 34)


def g_eraser(svg: Svg, ctx) -> None:
    top = svg.linear([(0, "#ffd0e0"), (1, "#f07aa0")], 0, 0, 0, 1)
    side = svg.linear([(0, "#4a8ae6"), (1, "#163a8a")], 0, 0, 0, 1)
    svg.add(path("M6 30l18 -18l14 14l-18 18z", fill=top, stroke="#7a2040", stroke_width=1.4, stroke_linejoin="round"))
    svg.add(path("M6 30l7 7l7 7l-7 -7z", fill="none"))
    svg.add(path("M6 30l7 7l11 -11l-7 -7z", fill=side, stroke="#0a2060", stroke_width=1.4, stroke_linejoin="round"))
    svg.add(path("M13 37l7 7", fill="none", stroke="#0a2060", stroke_width=1.4))
    svg.add(line(4, 44, 44, 44, stroke="#606060", stroke_width=1.6, stroke_linecap="round"))


def g_hresize(svg: Svg, ctx) -> None:
    straight_arrow(svg, 30, 24, 28, 0, "blue", 8, 10)
    straight_arrow(svg, 18, 24, 28, 180, "blue", 8, 10)


def g_vresize(svg: Svg, ctx) -> None:
    straight_arrow(svg, 24, 30, 28, 90, "blue", 8, 10)
    straight_arrow(svg, 24, 18, 28, 270, "blue", 8, 10)


def g_sound_select(svg: Svg, ctx) -> None:
    bar = svg.linear([(0, "#8ac4ff"), (1, "#1a5ac8")], 0, 0, 1, 0)
    svg.add(rect(30, 3, 10, 42, rx=2, fill=bar, stroke="#0a2a6a", stroke_width=1.2))
    lib = ctx.tabler
    t = "translate(0,4) scale(1.6)"
    svg.add(outline_group(lib, "pointer", "#202020", 3.4, t))
    svg.add(outline_group(lib, "pointer", "#ffffff", 1.6, t, fill="#ffffff"))


# --- small text badges (rasterised and laid over Nuvola pictures) -----------------------------------------
def g_badge_text(text: str, fg: str, bg: str | None = None, height: float = 15, width: float = 3.0):
    def draw(svg: Svg, ctx) -> None:
        if bg:
            _, total = text_elements(ctx.tabler, text, 0, 0, height, fg, width)
            w = min(46, total + 10)
            fill = svg.linear([(0, bg), (1, _shade(bg))], 0, 0, 0, 1)
            svg.add(rect(24 - w / 2, 24 - height / 2 - 5, w, height + 10, rx=4, fill=fill, stroke="#ffffff",
                         stroke_width=2))
            els, _ = text_elements(ctx.tabler, text, 24, 24 + height / 2, height, fg, width, anchor="middle")
        else:
            els, _ = text_elements(ctx.tabler, text, 24, 24 + height / 2, height, fg, width, anchor="middle",
                                   outline="#ffffff", outline_width=2.4)
        svg.body.extend(els)
    return draw


def _shade(color: str) -> str:
    r, g, b = int(color[1:3], 16), int(color[3:5], 16), int(color[5:7], 16)
    return "#%02x%02x%02x" % (int(r * 0.6), int(g * 0.6), int(b * 0.6))


# === Flat drawings (24 x 24) ====================================================================
FLAT_ACCENTS = {
    "red": "#e53935",
    "blue": "#1e88e5",
    "attack": "#ff3b30",
    "collision": "#2f7dff",
    "yellow": "#f5c400",
    "green": "#43a047",
    "orange": "#fb8c00",
    "cyan": "#00b8d4",
    "purple": "#8e44d8",
    "grey": "#9e9e9e",
}


def f_stroke(d: str, color: str, w: float = 2.0, **kw) -> str:
    return path(d, fill="none", stroke=color, stroke_width=w, stroke_linecap="round", stroke_linejoin="round", **kw)


def f_clsn_box(svg: Svg, x, y, w, h, kind: str, glyph: str, hatch: bool = True, sw: float = 1.6) -> None:
    col = FLAT_ACCENTS[kind]
    if hatch:
        svg.add(rect(x, y, w, h, fill=col + "40"))
    svg.add(rect(x, y, w, h, fill="none", stroke=col, stroke_width=sw, stroke_linejoin="round"))


def f_view_box(kind: str):
    def draw(svg: Svg, ctx, glyph: str) -> None:
        f_clsn_box(svg, 4, 4, 16, 16, kind, glyph, sw=2)
        for cx, cy in ((4, 4), (20, 4), (4, 20), (20, 20), (12, 4), (12, 20), (4, 12), (20, 12)):
            svg.add(rect(cx - 1.5, cy - 1.5, 3, 3, fill=glyph))
    return draw


def f_clsn_pair(svg: Svg, glyph: str, s: float = 1.0, ox: float = 0, oy: float = 0) -> None:
    f_clsn_box(svg, ox + 3 * s, oy + 3 * s, 12 * s, 12 * s, "attack", glyph)
    f_clsn_box(svg, ox + 9 * s, oy + 9 * s, 12 * s, 12 * s, "collision", glyph)


def f_clsn_add(kind: str):
    def draw(svg: Svg, ctx, glyph: str) -> None:
        f_clsn_box(svg, 3, 3, 14, 14, kind, glyph, sw=2)
    return draw


def f_clsn_delete(svg: Svg, ctx, glyph: str) -> None:
    f_clsn_pair(svg, glyph)


def f_clsn_delete_frame(svg: Svg, ctx, glyph: str) -> None:
    f_clsn_pair(svg, glyph, 0.75, 0, 0)
    f_clsn_pair(svg, glyph, 0.75, 5, 0)


def f_clsn_delete_anim(svg: Svg, ctx, glyph: str) -> None:
    svg.add(f_stroke("M4 5h16v14h-16zM8 5v14M16 5v14", glyph, 1.6))
    f_clsn_box(svg, 9.5, 8.5, 5, 7, "attack", glyph, sw=1.4)


def f_clsn_previous(svg: Svg, ctx, glyph: str) -> None:
    f_clsn_box(svg, 9, 9, 12, 12, "attack", glyph)
    f_clsn_box(svg, 12, 12, 9, 9, "collision", glyph)
    svg.add(f_stroke("M14 5h-6a4 4 0 0 0 -4 4v3M2 10l2 2l2 -2", glyph, 1.8))


def f_clsn_switch(svg: Svg, ctx, glyph: str) -> None:
    f_clsn_box(svg, 3, 3, 8, 8, "attack", glyph)
    f_clsn_box(svg, 13, 13, 8, 8, "collision", glyph)
    svg.add(f_stroke("M14 4h3a3 3 0 0 1 3 3v2l1.5 -1.5M20 9l-1.5 -1.5", glyph, 1.6))
    svg.add(f_stroke("M10 20h-3a3 3 0 0 1 -3 -3v-2l-1.5 1.5M4 15l1.5 1.5", glyph, 1.6))


def f_clsn_default(svg: Svg, ctx, glyph: str) -> None:
    svg.add(rect(3, 3, 12, 12, fill="none", stroke=FLAT_ACCENTS["attack"], stroke_width=1.6, stroke_dasharray="2.5 1.8"))
    svg.add(rect(9, 9, 12, 12, fill="none", stroke=FLAT_ACCENTS["collision"], stroke_width=1.6, stroke_dasharray="2.5 1.8"))
    els, _ = text_elements(ctx.tabler, "D", 12, 16, 8, glyph, 1.8, anchor="middle")
    svg.body.extend(els)


def _f_all(svg: Svg, ctx, glyph: str, inner) -> None:
    svg.add(f_stroke("M14 3v4a1 1 0 0 0 1 1h4M17 21h-10a2 2 0 0 1 -2 -2v-14a2 2 0 0 1 2 -2h7l5 5v6", glyph, 1.8))
    inner()
    svg.add(f_stroke("M16 16l3 3l-3 3M20 16l3 3l-3 3", glyph, 1.6))


def f_all(kind: str):
    def draw(svg: Svg, ctx, glyph: str) -> None:
        def inner():
            if kind == "time":
                svg.add(circle(11, 13, 4, fill="none", stroke=glyph, stroke_width=1.5))
                svg.add(f_stroke("M11 11v2l1.5 1", glyph, 1.4))
            elif kind == "flip":
                svg.add(f_stroke("M11 9v9", glyph, 1.2, stroke_dasharray="1.5 1.5"))
                svg.add(f_stroke("M9.5 10v7h-3.5zM12.5 10v7h3.5z", glyph, 1.3))
            elif kind == "offset":
                svg.add(f_stroke("M11 8.5v9M6.5 13h9M9.5 10l1.5 -1.5l1.5 1.5M9.5 16l1.5 1.5l1.5 -1.5M8 11.5l-1.5 1.5l1.5 1.5M14 11.5l1.5 1.5l-1.5 1.5", glyph, 1.2))
            elif kind == "scale":
                svg.add(f_stroke("M7 12v5h5M8 16l6 -6M11 10h3v3", glyph, 1.4))
            elif kind == "trans":
                for i in range(3):
                    for j in range(3):
                        if (i + j) % 2 == 0:
                            svg.add(rect(7.5 + i * 2.5, 10 + j * 2.5, 2.5, 2.5, fill=glyph))
                svg.add(rect(7.5, 10, 7.5, 7.5, fill="none", stroke=glyph, stroke_width=1.0))
        _f_all(svg, ctx, glyph, inner)
    return draw


def f_text(text: str, accent: str | None = None, height: float = 11, width: float = 2.0, italic: float = 8):
    def draw(svg: Svg, ctx, glyph: str) -> None:
        col = FLAT_ACCENTS.get(accent, accent) if accent else glyph
        _, total = text_elements(ctx.tabler, text, 0, 0, height, col, width, italic=italic)
        h = height if total <= 22 else height * 22 / total
        els, _ = text_elements(ctx.tabler, text, 12, 12 + h / 2, h, col, width, italic=italic, anchor="middle")
        svg.body.extend(els)
    return draw


def f_state(letter: str, accent: str):
    def draw(svg: Svg, ctx, glyph: str) -> None:
        svg.add(circle(12, 12, 10, fill=FLAT_ACCENTS[accent]))
        els, _ = text_elements(ctx.tabler, letter, 12, 17, 10, "#ffffff" if accent in ("red", "green", "blue", "purple") else "#1a1a1a", 2.2, anchor="middle")
        svg.body.extend(els)
    return draw


def f_dot(accent: str, badge=None):
    def draw(svg: Svg, ctx, glyph: str) -> None:
        svg.add(circle(12, 12, 8, fill=FLAT_ACCENTS.get(accent, accent)))
    return draw


def f_align(pos: str):
    def draw(svg: Svg, ctx, glyph: str) -> None:
        gx, gy = 12, 12
        w, h = 8, 10
        x = {"l": gx, "c": gx - w / 2, "r": gx - w}[pos[1]]
        y = {"t": gy, "c": gy - h / 2, "b": gy - h}[pos[0]]
        svg.add(f_stroke("M1 12h22M12 1v22", glyph, 1.0, stroke_dasharray="2 1.5", opacity="0.7"))
        svg.add(rect(x, y, w, h, rx=1, fill="none", stroke=glyph, stroke_width=1.6))
        svg.add(f_stroke(f"M{gx - 3} {gy}h6M{gx} {gy - 3}v6", FLAT_ACCENTS["yellow"], 1.8))
    return draw


def f_guide_lines(svg: Svg, ctx, glyph: str) -> None:
    svg.add(f_stroke("M3 15h18M9 3v18", glyph, 2))
    svg.add(f_stroke("M3 7h3M18 7h3M15 18v3", glyph, 1.4, stroke_dasharray="1.5 1.5"))


def f_selection_to(direction: str):
    def draw(svg: Svg, ctx, glyph: str) -> None:
        bx = 2 if direction == "left" else 13
        svg.add(rect(bx, 7, 9, 10, fill="none", stroke=glyph, stroke_width=1.6, stroke_dasharray="2 1.6"))
        if direction == "left":
            svg.add(f_stroke("M22 12h-8M17 9l-3 3l3 3", glyph, 2))
        else:
            svg.add(f_stroke("M2 12h8M7 9l3 3l-3 3", glyph, 2))
    return draw


def f_colors_to(direction: str):
    def draw(svg: Svg, ctx, glyph: str) -> None:
        bx = 4 if direction == "left" else 16
        for i, c in enumerate(("red", "yellow", "blue")):
            svg.add(circle(bx + (i % 2) * 3, 6 + i * 6, 2.6, fill=FLAT_ACCENTS[c]))
        if direction == "left":
            svg.add(f_stroke("M22 12h-9M16 9l-3 3l3 3", glyph, 2))
        else:
            svg.add(f_stroke("M2 12h9M8 9l3 3l-3 3", glyph, 2))
    return draw


def f_grid(colors: list[str]):
    def draw(svg: Svg, ctx, glyph: str) -> None:
        for j in range(3):
            for i in range(3):
                c = colors[(i + j * 3) % len(colors)]
                svg.add(circle(5 + i * 7, 5 + j * 7, 2.8, fill=FLAT_ACCENTS.get(c, c if c.startswith("#") else glyph)))
    return draw
