"""The Sources PDF in each icon set folder.

The text uses a few Markdown-like lines: "# title", "## heading", "- item", "| table |" rows and
plain paragraphs. SOURCE_DATE_EPOCH sets the date stored in the file.
"""
from __future__ import annotations

import datetime as dt
import os
import re
import uuid
from pathlib import Path

FONT = "Roboto"


def when() -> dt.datetime:
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    if epoch:
        return dt.datetime.fromtimestamp(int(epoch), dt.timezone.utc)
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0)


def parse(text: str) -> list[tuple[str, object]]:
    items: list[tuple[str, object]] = []
    for line in text.splitlines():
        if not line.strip():
            continue
        if line.startswith("## "):
            items.append(("h2", line[3:]))
        elif line.startswith("# "):
            items.append(("h1", line[2:]))
        elif line.startswith("- "):
            items.append(("li", line[2:]))
        elif line.startswith("|"):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if all(re.fullmatch(r"-{3,}", c) for c in cells):
                continue
            if items and items[-1][0] == "table":
                items[-1][1].append(cells)
            else:
                items.append(("table", [cells]))
        else:
            items.append(("p", line))
    return items


SPANS = re.compile(r"(\*\*[^*]+\*\*|`[^`]+`|\*[^*]+\*)")


def spans(text: str) -> list[tuple[str, str]]:
    """(text, "b" bold, "i" italic or "" plain) pieces of a line."""
    out = []
    for piece in SPANS.split(text):
        if not piece:
            continue
        if piece.startswith("**"):
            out.append((piece[2:-2], "b"))
        elif piece.startswith("`"):
            out.append((piece[1:-1], ""))
        elif piece.startswith("*") and len(piece) > 2:
            out.append((piece[1:-1], "i"))
        else:
            out.append((piece, ""))
    return out


def _html(text: str) -> str:
    def inline(line):
        out = []
        for piece, kind in spans(line):
            piece = piece.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
            out.append(f"<{kind}>{piece}</{kind}>" if kind else piece)
        return "".join(out)

    parts = []
    for kind, value in parse(text):
        if kind == "h1":
            parts.append(f"<h1>{inline(value)}</h1>")
        elif kind == "h2":
            parts.append(f"<h2>{inline(value)}</h2>")
        elif kind == "li":
            if parts and parts[-1].endswith("</ul>"):
                parts[-1] = parts[-1][:-5] + f"<li>{inline(value)}</li></ul>"
            else:
                parts.append(f"<ul><li>{inline(value)}</li></ul>")
        elif kind == "p":
            parts.append(f"<p>{inline(value)}</p>")
        else:
            rows = []
            for k, row in enumerate(value):
                cell = '<th align="left">{}</th>' if k == 0 else "<td>{}</td>"
                rows.append("<tr>" + "".join(cell.format(inline(c)) for c in row) + "</tr>")
            parts.append('<table border="1" cellspacing="0" cellpadding="3" width="100%">' + "".join(rows)
                         + "</table>")
    return "<html><body>" + "".join(parts) + "</body></html>"


def _load_roboto() -> None:
    # Qt's offscreen platform (used by art.py) sees no system fonts, so add the font files ourselves.
    from PySide6.QtGui import QFontDatabase

    if FONT in QFontDatabase.families():
        return
    home = Path.home()
    folders = [home / ".local/share/fonts", home / ".fonts", Path("/usr/share/fonts"), Path("/usr/local/share/fonts"),
               home / "Library/Fonts", Path("/Library/Fonts")]
    if os.environ.get("LOCALAPPDATA"):
        folders.insert(0, Path(os.environ["LOCALAPPDATA"]) / "Microsoft/Windows/Fonts")
    if os.environ.get("WINDIR"):
        folders.insert(1, Path(os.environ["WINDIR"]) / "Fonts")
    for folder in folders:
        if folder.is_dir():
            for f in sorted(folder.rglob("Roboto-*.ttf")):
                QFontDatabase.addApplicationFont(str(f))
    if FONT not in QFontDatabase.families():
        raise SystemExit("The Sources PDF needs the Roboto font. Install it and run again.")


def pdf(text: str, title: str) -> bytes:
    from PySide6.QtCore import QBuffer, QByteArray, QIODevice, QMarginsF, QUuid
    from PySide6.QtGui import QFont, QGuiApplication, QPageLayout, QPageSize, QPdfWriter, QTextDocument

    _app = QGuiApplication.instance() or QGuiApplication([])
    _load_roboto()
    data = QByteArray()
    buffer = QBuffer(data)
    buffer.open(QIODevice.WriteOnly)
    writer = QPdfWriter(buffer)
    writer.setTitle(title)
    writer.setCreator("Open Factory Studio")
    writer.setDocumentId(QUuid(str(uuid.uuid5(uuid.NAMESPACE_URL, title + "\n" + text))))
    writer.setPageLayout(QPageLayout(QPageSize(QPageSize.A4), QPageLayout.Portrait, QMarginsF(20, 20, 20, 20),
                                     QPageLayout.Millimeter))
    doc = QTextDocument()
    doc.setDefaultFont(QFont(FONT, 9))
    doc.setHtml(_html(text))
    doc.print_(writer)
    del writer
    buffer.close()
    out = bytes(data)
    # Qt dates the file now. Put the chosen date in its place, keeping the length so offsets stay right.
    local = when().astimezone()
    stamp = local.strftime("%Y%m%d%H%M%S")
    out = re.sub(rb"(/CreationDate \(D:)\d{14}", lambda m: m.group(1) + stamp.encode(), out)
    iso = local.strftime("%Y-%m-%dT%H:%M:%S").encode()
    out = re.sub(rb"(<xmp:(?:CreateDate|ModifyDate|MetadataDate)>)\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d",
                 lambda m: m.group(1) + iso, out)
    return out
