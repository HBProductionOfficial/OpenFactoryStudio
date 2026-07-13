"""Drawing helpers for build_iconsets.py. The SVG output only uses what Qt SVG can render."""
from __future__ import annotations

import io
import math
import re
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image, ImageChops, ImageEnhance, ImageFilter

SVG_NS = "http://www.w3.org/2000/svg"


def fmt(v: float) -> str:
    s = f"{v:.3f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


# --- SVG writer ---------------------------------------------------------------------------------
class Svg:
    def __init__(self, size: int = 48, view: float | None = None):
        self.size = size
        self.view = view if view is not None else size
        self.defs: list[str] = []
        self.body: list[str] = []
        self._n = 0

    def uid(self, prefix: str = "g") -> str:
        self._n += 1
        return f"{prefix}{self._n}"

    def linear(self, stops, x1=0.0, y1=0.0, x2=0.0, y2=1.0, user: bool = False) -> str:
        gid = self.uid("l")
        units = ' gradientUnits="userSpaceOnUse"' if user else ""
        s = "".join(_stop(o, c) for o, c in stops)
        self.defs.append(f'<linearGradient id="{gid}" x1="{fmt(x1)}" y1="{fmt(y1)}" x2="{fmt(x2)}" '
                         f'y2="{fmt(y2)}"{units}>{s}</linearGradient>')
        return f"url(#{gid})"

    def radial(self, stops, cx=0.5, cy=0.5, r=0.5, fx=None, fy=None, user: bool = False) -> str:
        gid = self.uid("r")
        fx = cx if fx is None else fx
        fy = cy if fy is None else fy
        units = ' gradientUnits="userSpaceOnUse"' if user else ""
        s = "".join(_stop(o, c) for o, c in stops)
        self.defs.append(f'<radialGradient id="{gid}" cx="{fmt(cx)}" cy="{fmt(cy)}" r="{fmt(r)}" '
                         f'fx="{fmt(fx)}" fy="{fmt(fy)}"{units}>{s}</radialGradient>')
        return f"url(#{gid})"

    def add(self, element: str) -> None:
        self.body.append(element)

    def group(self, content: list[str] | str, transform: str = "", **attrs) -> None:
        if isinstance(content, list):
            content = "\n".join(content)
        a = f' transform="{transform}"' if transform else ""
        a += "".join(f' {k.replace("_", "-")}="{v}"' for k, v in attrs.items())
        self.body.append(f"<g{a}>\n{content}\n</g>")

    def render(self) -> str:
        out = [f'<svg xmlns="{SVG_NS}" width="{self.size}" height="{self.size}" '
               f'viewBox="0 0 {fmt(self.view)} {fmt(self.view)}">']
        if self.defs:
            out.append("<defs>")
            out.extend(self.defs)
            out.append("</defs>")
        out.extend(self.body)
        out.append("</svg>")
        return "\n".join(out) + "\n"


def _stop(offset: float, color: str) -> str:
    if len(color) == 9:  # #rrggbbaa
        a = int(color[7:9], 16) / 255
        return f'<stop offset="{fmt(offset)}" stop-color="{color[:7]}" stop-opacity="{fmt(a)}"/>'
    return f'<stop offset="{fmt(offset)}" stop-color="{color}"/>'


def attrs(**kw) -> str:
    """Qt SVG does not read '#rrggbbaa', so those paints become a colour plus an opacity."""
    out = []
    for k, v in kw.items():
        if v is None:
            continue
        name = k.rstrip("_").replace("_", "-")
        if name in ("fill", "stroke") and isinstance(v, str) and len(v) == 9 and v.startswith("#"):
            out.append(f' {name}="{v[:7]}" {name}-opacity="{fmt(int(v[7:], 16) / 255)}"')
            continue
        out.append(f' {name}="{fmt(v) if isinstance(v, (int, float)) else v}"')
    return "".join(out)


def path(d: str, **kw) -> str:
    return f'<path d="{d}"{attrs(**kw)}/>'


def rect(x, y, w, h, **kw) -> str:
    return f'<rect x="{fmt(x)}" y="{fmt(y)}" width="{fmt(w)}" height="{fmt(h)}"{attrs(**kw)}/>'


def circle(cx, cy, r, **kw) -> str:
    return f'<circle cx="{fmt(cx)}" cy="{fmt(cy)}" r="{fmt(r)}"{attrs(**kw)}/>'


def ellipse(cx, cy, rx, ry, **kw) -> str:
    return f'<ellipse cx="{fmt(cx)}" cy="{fmt(cy)}" rx="{fmt(rx)}" ry="{fmt(ry)}"{attrs(**kw)}/>'


def line(x1, y1, x2, y2, **kw) -> str:
    return f'<line x1="{fmt(x1)}" y1="{fmt(y1)}" x2="{fmt(x2)}" y2="{fmt(y2)}"{attrs(**kw)}/>'


def poly(points, closed: bool = True, **kw) -> str:
    pts = " ".join(f"{fmt(x)},{fmt(y)}" for x, y in points)
    tag = "polygon" if closed else "polyline"
    return f'<{tag} points="{pts}"{attrs(**kw)}/>'


# --- Outline icon sets (Tabler, Lucide): parsing and glyph geometry -----------------------------
_GEOMETRY = ("d", "cx", "cy", "r", "rx", "ry", "x", "y", "width", "height", "x1", "y1", "x2", "y2", "points",
             "fill", "stroke", "stroke-width", "transform")


class OutlineLibrary:
    def __init__(self, roots: list[Path]):
        self.roots = roots
        self._cache: dict[str, list[str]] = {}

    def find(self, name: str) -> Path:
        for root in self.roots:
            p = root / f"{name}.svg"
            if p.is_file():
                return p
        raise KeyError(f"outline icon not found: {name}")

    def elements(self, name: str) -> list[str]:
        """Returns the drawing elements of an icon with colours left as currentColor."""
        if name in self._cache:
            return self._cache[name]
        tree = ET.parse(self.find(name))
        out = []
        for el in tree.getroot():
            tag = el.tag.split("}")[-1]
            if tag not in ("path", "circle", "rect", "line", "polyline", "polygon", "ellipse"):
                continue
            if tag == "path" and el.get("d", "").replace(" ", "") in ("M00h24v24H0z",):
                continue
            a = "".join(f' {k}="{el.get(k)}"' for k in _GEOMETRY if el.get(k) is not None)
            out.append(f"<{tag}{a}/>")
        self._cache[name] = out
        return out


def outline_group(lib: OutlineLibrary, name: str, color: str, width: float = 2.0, transform: str = "",
                  fill: str = "none") -> str:
    els = "\n".join(e.replace("currentColor", color) for e in lib.elements(name))
    t = f' transform="{transform}"' if transform else ""
    return (f'<g{t} fill="{fill}" stroke="{color}" stroke-width="{fmt(width)}" stroke-linecap="round" '
            f'stroke-linejoin="round">\n{els}\n</g>')


_NUM = re.compile(r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")
_CMD = re.compile(r"[MmLlHhVvCcSsQqTtAaZz]|[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")
_ARGS = {"M": 2, "L": 2, "H": 1, "V": 1, "C": 6, "S": 4, "Q": 4, "T": 2, "A": 7, "Z": 0}


def path_points(d: str) -> list[tuple[float, float]]:
    """Approximate outline points of a path (end points, control points, arc extremes)."""
    toks = _CMD.findall(d)
    pts = []
    x = y = sx = sy = 0.0
    i = 0
    cmd = None
    while i < len(toks):
        t = toks[i]
        if t.isalpha():
            cmd = t
            i += 1
            if cmd in "Zz":
                x, y = sx, sy
                pts.append((x, y))
                continue
        n = _ARGS[cmd.upper()]
        args = [float(v) for v in toks[i:i + n]]
        i += n
        rel = cmd.islower()
        c = cmd.upper()
        if c == "M":
            x, y = (x + args[0], y + args[1]) if rel else (args[0], args[1])
            sx, sy = x, y
            pts.append((x, y))
            cmd = "l" if rel else "L"
        elif c == "L" or c == "T":
            x, y = (x + args[0], y + args[1]) if rel else (args[0], args[1])
            pts.append((x, y))
        elif c == "H":
            x = x + args[0] if rel else args[0]
            pts.append((x, y))
        elif c == "V":
            y = y + args[0] if rel else args[0]
            pts.append((x, y))
        elif c in "CSQ":
            for k in range(0, n, 2):
                px, py = (x + args[k], y + args[k + 1]) if rel else (args[k], args[k + 1])
                pts.append((px, py))
            x, y = pts[-1]
        elif c == "A":
            r = max(abs(args[0]), abs(args[1]))
            nx, ny = (x + args[5], y + args[6]) if rel else (args[5], args[6])
            mx, my = (x + nx) / 2, (y + ny) / 2
            pts.extend([(nx, ny), (mx, my)])
            # keep the bulge roughly inside the bounds
            dx, dy = nx - x, ny - y
            dist = math.hypot(dx, dy) or 1.0
            sweep = 1 if args[4] else -1
            bulge = min(r, dist / 2) if not args[3] else r
            pts.append((mx - dy / dist * bulge * sweep, my + dx / dist * bulge * sweep))
            x, y = nx, ny
    return pts


def element_points(el: str) -> list[tuple[float, float]]:
    def g(name):
        m = re.search(rf'\s{name}="([^"]*)"', el)
        return m.group(1) if m else None

    if el.startswith("<path"):
        return path_points(g("d"))
    if el.startswith("<circle"):
        cx, cy, r = float(g("cx")), float(g("cy")), float(g("r"))
        return [(cx - r, cy - r), (cx + r, cy + r)]
    if el.startswith("<ellipse"):
        cx, cy, rx, ry = float(g("cx")), float(g("cy")), float(g("rx")), float(g("ry"))
        return [(cx - rx, cy - ry), (cx + rx, cy + ry)]
    if el.startswith("<rect"):
        x, y, w, h = (float(g(k) or 0) for k in ("x", "y", "width", "height"))
        return [(x, y), (x + w, y + h)]
    if el.startswith("<line"):
        return [(float(g("x1")), float(g("y1"))), (float(g("x2")), float(g("y2")))]
    if el.startswith("<poly"):
        nums = [float(v) for v in _NUM.findall(g("points"))]
        return list(zip(nums[::2], nums[1::2]))
    return []


def outline_bounds(lib: OutlineLibrary, name: str) -> tuple[float, float, float, float]:
    pts = [p for e in lib.elements(name) for p in element_points(e)]
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return min(xs), min(ys), max(xs), max(ys)


def glyph_name(ch: str) -> str:
    if ch.isdigit():
        return f"number-{ch}"
    if ch.isalpha():
        return f"letter-{ch.lower()}"
    symbols = {"*": "asterisk-simple", "/": "slash", "+": "plus", "-": "minus"}
    if ch in symbols:
        return symbols[ch]
    raise KeyError(f"no glyph for {ch!r}")


def _tokens(text: str) -> list[str]:
    """Splits text into characters; "{name}" stands for a whole outline icon used as a glyph."""
    out, i = [], 0
    while i < len(text):
        if text[i] == "{":
            j = text.index("}", i)
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(text[i])
            i += 1
    return out


def text_elements(lib: OutlineLibrary, text: str, x: float, y: float, height: float, color: str,
                  width: float = 2.0, gap: float = 1.6, italic: float = 0.0, anchor: str = "start",
                  outline: str | None = None, outline_width: float = 0.0) -> tuple[list[str], float]:
    """Lays out text with the letter-* and number-* outline icons. Glyphs are 16 units tall in their
    24 unit box. `height` is the cap height, `y` the baseline. Returns (elements, advance width)."""
    s = height / 16.0
    slant = math.tan(math.radians(italic)) if italic else 0.0
    items = []
    pen = 0.0
    for tok in _tokens(text):
        if tok == " ":
            pen += 6 * s
            continue
        if tok in ".,;:":
            items.append((tok, pen))
            pen += 3.5 * s + gap * s
            continue
        name = tok[1:-1] if tok.startswith("{") else glyph_name(tok)
        x0, _, x1, _ = outline_bounds(lib, name)
        items.append((name, pen - x0 * s))
        pen += (x1 - x0) * s + gap * s
    total = pen - gap * s
    if anchor == "middle":
        x -= total / 2
    elif anchor == "end":
        x -= total
    out = []
    for name, off in items:
        tx = x + off
        ty = y - 20 * s
        if name in ".,;:":
            out.extend(_punct(name, tx, y, s, color, width, slant, outline, outline_width))
            continue
        if slant:
            base = f"translate({fmt(tx + slant * 12 * s)},{fmt(ty)}) skewX({fmt(-italic)}) scale({fmt(s)})"
        else:
            base = f"translate({fmt(tx)},{fmt(ty)}) scale({fmt(s)})"
        if outline:
            out.append(outline_group(lib, name, outline, (width + outline_width) / s, base))
        out.append(outline_group(lib, name, color, width / s, base))
    return out, total


def _punct(ch, x, baseline, s, color, width, slant, outline, outline_width):
    els = []
    dots = {".": [(1.2, 0.0)], ":": [(1.2, 0.0), (1.2, -8.0)], ";": [(1.2, -8.0)], ",": []}[ch]
    for dx, dy in dots:
        v = 20 + dy
        cx = x + dx * s + slant * (12 - v) * s
        cy = baseline + dy * s - width * 0.1
        if outline:
            els.append(circle(cx, cy, (width + outline_width) * 0.72, fill=outline))
        els.append(circle(cx, cy, width * 0.72, fill=color))
    if ch in ",;":
        cx = x + 1.4 * s + slant * (12 - 20) * s
        d = f"M{fmt(cx)} {fmt(baseline)} l{fmt(-1.0 * s)} {fmt(3.0 * s)}"
        if outline:
            els.append(path(d, fill="none", stroke=outline, stroke_width=width + outline_width, stroke_linecap="round"))
        els.append(path(d, fill="none", stroke=color, stroke_width=width, stroke_linecap="round"))
    return els


def icon_glyph(lib: OutlineLibrary, name: str, cx: float, cy: float, target_w: float, color: str,
               width: float = 3.0, italic: float = 0.0, outline: str | None = None,
               outline_width: float = 0.0, max_h: float | None = None) -> list[str]:
    """Draws a whole outline icon (e.g. "abc") scaled so that its drawing is target_w wide."""
    x0, y0, x1, y1 = outline_bounds(lib, name)
    s = target_w / (x1 - x0)
    if max_h and (y1 - y0) * s > max_h:
        s = max_h / (y1 - y0)
    mx, my = (x0 + x1) / 2, (y0 + y1) / 2
    skew = f" skewX({fmt(-italic)})" if italic else ""
    t = f"translate({fmt(cx)},{fmt(cy)}){skew} scale({fmt(s)}) translate({fmt(-mx)},{fmt(-my)})"
    out = []
    if outline:
        out.append(outline_group(lib, name, outline, (width + outline_width) / s, t))
    out.append(outline_group(lib, name, color, width / s, t))
    return out


# --- Glossy primitives (48 x 48 canvas, Nuvola-like rendering) ----------------------------------
BALL_COLORS = {
    # light, base, dark
    "blue": ("#bfe3ff", "#2f86e6", "#0b3a8c"),
    "green": ("#d2ffb8", "#3cbc34", "#11590f"),
    "red": ("#ffc4b4", "#e8341f", "#760b04"),
    "orange": ("#ffe0b0", "#f5821e", "#8c3504"),
    "yellow": ("#fffbc4", "#f6cf1e", "#8c6604"),
    "grey": ("#ffffff", "#a4a4a4", "#454545"),
    "cyan": ("#c8f8ff", "#1fb8dc", "#075a72"),
    "purple": ("#f0d0ff", "#9a4ee0", "#41106e"),
    "brown": ("#e8cfc0", "#8a5a46", "#3a2016"),
}


def ball(svg: Svg, cx: float, cy: float, r: float, color: str = "blue") -> None:
    light, base, dark = BALL_COLORS[color]
    fill = svg.radial([(0, light), (0.55, base), (1, dark)], cx - r * 0.25, cy - r * 0.3, r * 1.25, user=True)
    svg.add(circle(cx, cy, r, fill=fill, stroke=dark, stroke_width=r * 0.07))
    hl = svg.linear([(0, "#ffffffe6"), (1, "#ffffff00")], 0, cy - r * 0.92, 0, cy + r * 0.05, user=True)
    svg.add(ellipse(cx, cy - r * 0.45, r * 0.68, r * 0.45, fill=hl))


def sheet(svg: Svg, x: float, y: float, w: float, h: float, fold: float = 9.0, lines: int = 0) -> None:
    """A white document page with a folded top-right corner."""
    fill = svg.linear([(0, "#ffffff"), (1, "#dcdcdc")], 0, 0, 1, 1)
    d = (f"M{fmt(x)} {fmt(y)}h{fmt(w - fold)}l{fmt(fold)} {fmt(fold)}v{fmt(h - fold)}h{fmt(-w)}z")
    svg.add(path(d, fill="#0000001a", transform="translate(1.2,1.2)"))
    svg.add(path(d, fill=fill, stroke="#7c7c7c", stroke_width=1.2, stroke_linejoin="round"))
    svg.add(path(f"M{fmt(x + w - fold)} {fmt(y)}v{fmt(fold)}h{fmt(fold)}", fill="#c8c8c8", stroke="#7c7c7c",
                 stroke_width=1.0, stroke_linejoin="round"))
    for i in range(lines):
        ly = y + fold + 4 + i * 4.2
        if ly > y + h - 3:
            break
        lw = (w - 8) * (0.65 if i % 3 == 2 else 1.0)
        svg.add(line(x + 4, ly, x + 4 + lw, ly, stroke="#9aa6b8", stroke_width=1.6, stroke_linecap="round"))


def sparkle(svg: Svg, cx: float, cy: float, r: float) -> None:
    """The orange-yellow 'new item' star burst."""
    pts = []
    for i in range(16):
        a = math.radians(i * 22.5 - 90)
        rr = r if i % 2 == 0 else r * (0.42 if i % 4 == 1 else 0.38)
        if i % 4 == 2:
            rr = r * 0.72
        pts.append((cx + math.cos(a) * rr, cy + math.sin(a) * rr))
    fill = svg.radial([(0, "#fffde0"), (0.35, "#ffe23a"), (1, "#ff7a00")], cx, cy, r, user=True)
    svg.add(poly(pts, fill=fill, stroke="#c84c00", stroke_width=max(0.6, r * 0.07), stroke_linejoin="round"))
    svg.add(circle(cx, cy, r * 0.22, fill="#ffffff"))


def red_x(svg: Svg, cx: float, cy: float, r: float) -> None:
    t = r * 0.42
    d = (f"M{fmt(cx - r)} {fmt(cy - r + t)}l{fmt(t)} {fmt(-t)}l{fmt(r - t)} {fmt(r - t)}l{fmt(r - t)} {fmt(-(r - t))}"
         f"l{fmt(t)} {fmt(t)}l{fmt(-(r - t))} {fmt(r - t)}l{fmt(r - t)} {fmt(r - t)}l{fmt(-t)} {fmt(t)}"
         f"l{fmt(-(r - t))} {fmt(-(r - t))}l{fmt(-(r - t))} {fmt(r - t)}l{fmt(-t)} {fmt(-t)}l{fmt(r - t)} {fmt(-(r - t))}z")
    fill = svg.linear([(0, "#ff7a6a"), (0.5, "#e01e10"), (1, "#9a0800")], 0, 0, 0, 1)
    svg.add(path(d, fill=fill, stroke="#6a0400", stroke_width=max(0.8, r * 0.09), stroke_linejoin="round"))


def clsn_box(svg: Svg, x: float, y: float, w: float, h: float, kind: str, width: float = 3.0) -> None:
    """A collision box: kind 'attack' (red, CLSN1) or 'collision' (blue, CLSN2)."""
    col, light = (("#d80000", "#ff6a6a") if kind == "attack" else ("#0016d8", "#6a7aff"))
    svg.add(rect(x, y, w, h, fill=col + "30", stroke=col, stroke_width=width))
    svg.add(rect(x + width * 0.5, y + width * 0.5, w - width, h - width, fill="none", stroke=light,
                 stroke_width=width * 0.3))


def bold_arrow(svg: Svg, d: str, color: str = "blue", width: float = 1.4) -> None:
    light, base, dark = BALL_COLORS[color]
    fill = svg.linear([(0, light), (0.5, base), (1, dark)], 0, 0, 0, 1)
    svg.add(path(d, fill=fill, stroke=dark, stroke_width=width, stroke_linejoin="round"))


def curved_arrow(svg: Svg, pts: list[tuple[float, float]], color: str = "blue", w: float = 6.0,
                 head: float = 7.0) -> None:
    """A thick arrow along a quadratic curve p0 -> p2 (control p1), drawn as a filled outline."""
    (x0, y0), (x1, y1), (x2, y2) = pts
    samples = []
    for i in range(21):
        t = i / 20
        bx = (1 - t) ** 2 * x0 + 2 * (1 - t) * t * x1 + t * t * x2
        by = (1 - t) ** 2 * y0 + 2 * (1 - t) * t * y1 + t * t * y2
        samples.append((bx, by))
    ex, ey = samples[-1]
    px, py = samples[-3]
    ang = math.atan2(ey - py, ex - px)
    # shorten the shaft so the head covers its end
    back = head * 0.8
    shaft = [p for p in samples if math.hypot(p[0] - ex, p[1] - ey) > back]
    left, right = [], []
    for i, (bx, by) in enumerate(shaft):
        nx_, ny_ = shaft[min(i + 1, len(shaft) - 1)]
        qx, qy = shaft[max(i - 1, 0)]
        a = math.atan2(ny_ - qy, nx_ - qx)
        left.append((bx + math.sin(a) * w / 2, by - math.cos(a) * w / 2))
        right.append((bx - math.sin(a) * w / 2, by + math.cos(a) * w / 2))
    hx, hy = shaft[-1]
    tip = (ex + math.cos(ang) * head * 0.25, ey + math.sin(ang) * head * 0.25)
    hl = (hx + math.sin(ang) * head, hy - math.cos(ang) * head)
    hr = (hx - math.sin(ang) * head, hy + math.cos(ang) * head)
    outline = left + [hl, tip, hr] + right[::-1]
    light, base, dark = BALL_COLORS[color]
    fill = svg.linear([(0, light), (0.45, base), (1, dark)], 0, 0, 0, 1)
    svg.add(poly(outline, fill=fill, stroke=dark, stroke_width=1.3, stroke_linejoin="round"))


def straight_arrow(svg: Svg, cx: float, cy: float, length: float, angle: float, color: str = "blue",
                   w: float = 9.0, head: float = 9.0) -> None:
    """A bold straight arrow centred on (cx, cy) pointing at `angle` degrees (0 = right)."""
    a = math.radians(angle)
    ux, uy = math.cos(a), math.sin(a)
    px, py = -uy, ux
    half = length / 2
    tail = (cx - ux * half, cy - uy * half)
    tip = (cx + ux * half, cy + uy * half)
    neck = (tip[0] - ux * head, tip[1] - uy * head)
    pts = [
        (tail[0] + px * w / 2, tail[1] + py * w / 2),
        (neck[0] + px * w / 2, neck[1] + py * w / 2),
        (neck[0] + px * w, neck[1] + py * w),
        tip,
        (neck[0] - px * w, neck[1] - py * w),
        (neck[0] - px * w / 2, neck[1] - py * w / 2),
        (tail[0] - px * w / 2, tail[1] - py * w / 2),
    ]
    light, base, dark = BALL_COLORS[color]
    fill = svg.linear([(0, light), (0.5, base), (1, dark)], 0, 0, 0, 1)
    svg.add(poly(pts, fill=fill, stroke=dark, stroke_width=1.3, stroke_linejoin="round"))


def glossy_text(svg: Svg, lib: OutlineLibrary, text: str, cx: float, baseline: float, height: float,
                color: str, width: float = 3.4, italic: float = 10.0, max_width: float = 44.0) -> None:
    """Bold coloured text with a darker rim, centred on cx (shrinks to fit max_width)."""
    _, total = text_elements(lib, text, 0, 0, height, color, width, italic=italic)
    if total > max_width:
        height *= max_width / total
        width *= max(0.75, max_width / total)
    dark = _darken(color, 0.55)
    els, _ = text_elements(lib, text, cx, baseline, height, color, width, italic=italic, anchor="middle",
                           outline=dark, outline_width=1.6)
    svg.body.extend(els)


def _darken(color: str, f: float) -> str:
    r, g, b = int(color[1:3], 16), int(color[3:5], 16), int(color[5:7], 16)
    return "#%02x%02x%02x" % (int(r * f), int(g * f), int(b * f))


# --- Raster helpers -----------------------------------------------------------------------------
def png_bytes(img: Image.Image) -> bytes:
    buf = io.BytesIO()
    img.save(buf, format="PNG", optimize=True)
    return buf.getvalue()


def fit(img: Image.Image, size: int) -> Image.Image:
    img = img.convert("RGBA")
    if img.size == (size, size):
        return img.copy()
    w, h = img.size
    f = size / max(w, h)
    out = img.resize((max(1, round(w * f)), max(1, round(h * f))), Image.LANCZOS)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.alpha_composite(out, ((size - out.width) // 2, (size - out.height) // 2))
    return canvas


def overlay(base: Image.Image, top: Image.Image, scale: float, anchor: str = "br", margin: int = 0,
            offset: tuple[int, int] = (0, 0)) -> Image.Image:
    """Pastes `top` scaled to `scale` of the canvas at a corner (br, bl, tr, tl, c)."""
    size = base.width
    side = max(1, round(size * scale))
    t = fit(top, side)
    x = {"l": margin, "r": size - side - margin, "c": (size - side) // 2}
    y = {"t": margin, "b": size - side - margin, "c": (size - side) // 2}
    pos = (x[anchor[1] if len(anchor) == 2 else "c"] + offset[0], y[anchor[0] if len(anchor) == 2 else "c"] + offset[1])
    out = base.copy()
    # a soft dark halo keeps the badge readable over the picture below
    halo = Image.new("RGBA", out.size, (0, 0, 0, 0))
    alpha = t.getchannel("A").filter(ImageFilter.MaxFilter(3))
    shadow = Image.new("RGBA", t.size, (255, 255, 255, 0))
    shadow.putalpha(alpha.point(lambda a: int(a * 0.55)))
    halo.alpha_composite(shadow, pos)
    out.alpha_composite(halo)
    out.alpha_composite(t, pos)
    return out


def hue_shift(img: Image.Image, degrees: float) -> Image.Image:
    rgba = img.convert("RGBA")
    a = rgba.getchannel("A")
    h, s, v = rgba.convert("RGB").convert("HSV").split()
    shift = int(round(degrees / 360 * 255)) % 256
    h = h.point(lambda p: (p + shift) % 256)
    out = Image.merge("HSV", (h, s, v)).convert("RGB").convert("RGBA")
    out.putalpha(a)
    return out


def desaturate(img: Image.Image, amount: float = 1.0, brightness: float = 1.0) -> Image.Image:
    rgba = img.convert("RGBA")
    a = rgba.getchannel("A")
    out = ImageEnhance.Color(rgba.convert("RGB")).enhance(1.0 - amount)
    if brightness != 1.0:
        out = ImageEnhance.Brightness(out).enhance(brightness)
    out = out.convert("RGBA")
    out.putalpha(a)
    return out


def mirror(img: Image.Image) -> Image.Image:
    return img.transpose(Image.FLIP_LEFT_RIGHT)


def stack(img: Image.Image, size: int, count: int = 2, step: int = 6) -> Image.Image:
    side = size - step * (count - 1)
    t = fit(img, side)
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    for i in range(count):
        out.alpha_composite(t, (step * (count - 1 - i), step * i))
    return out


_qt_app = None


def render_svg(text: str, size: int) -> Image.Image:
    global _qt_app
    from PySide6.QtCore import QByteArray, QRectF, Qt
    from PySide6.QtGui import QGuiApplication, QImage, QPainter
    from PySide6.QtSvg import QSvgRenderer

    if _qt_app is None:
        import os
        os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
        _qt_app = QGuiApplication.instance() or QGuiApplication([])
    renderer = QSvgRenderer(QByteArray(text.encode("utf-8")))
    img = QImage(size, size, QImage.Format_RGBA8888)
    img.fill(Qt.transparent)
    p = QPainter(img)
    p.setRenderHint(QPainter.Antialiasing)
    p.setRenderHint(QPainter.SmoothPixmapTransform)
    renderer.render(p, QRectF(0, 0, size, size))
    p.end()
    data = bytes(img.constBits())
    return Image.frombuffer("RGBA", (size, size), data, "raw", "RGBA", img.bytesPerLine(), 1).copy()


def same_pixels(a: Image.Image, b: Image.Image) -> bool:
    return a.size == b.size and ImageChops.difference(a.convert("RGBA"), b.convert("RGBA")).getbbox() is None
