#!/usr/bin/env python3
"""Writes data/engines/mugen/docs/<lang>/index.html. Run it after adding or renaming pages."""
import html
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir)
DOCS = os.path.join(ROOT, 'data', 'engines', 'mugen', 'docs')


def title_of(path):
    with open(path, encoding='utf-8') as f:
        text = f.read()
    m = re.search(r'<h2>(.*?)</h2>', text, re.S)
    return html.unescape(re.sub(r'<[^>]+>', '', m.group(1))).strip() if m else os.path.basename(path)


def section(lang_dir, kind, heading):
    common = os.path.join(lang_dir, 'common')
    pages = sorted(f for f in os.listdir(common) if f.startswith(kind + '.') and f.endswith('.html'))
    rows = []
    for page in pages:
        name = title_of(os.path.join(common, page))
        variants = [v for v in sorted(os.listdir(lang_dir))
                    if v != 'common' and os.path.isfile(os.path.join(lang_dir, v, page))]
        extra = ''.join(' <a href="%s/%s">(%s)</a>' % (v, page, v) for v in variants)
        rows.append('  <li><a href="common/%s">%s</a>%s</li>' % (page, html.escape(name), extra))
    return '<h2>%s</h2>\n<ul class="columns">\n%s\n</ul>\n' % (heading, '\n'.join(rows))


def main():
    for lang in sorted(os.listdir(DOCS)):
        lang_dir = os.path.join(DOCS, lang)
        if not os.path.isdir(os.path.join(lang_dir, 'common')):
            continue
        body = (section(lang_dir, 'sctrl', 'State controllers') + section(lang_dir, 'trigger', 'Triggers'))
        page = ('<!DOCTYPE html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
                '<title>M.U.G.E.N reference</title>\n'
                '<style>body{font-family:"Segoe UI",Helvetica,Arial,sans-serif;max-width:960px;margin:0 auto;'
                'padding:24px 32px}ul.columns{columns:3;list-style:none;padding:0}'
                'ul.columns li{margin:0.15em 0}</style>\n</head>\n<body>\n'
                '<h1>M.U.G.E.N reference</h1>\n'
                '<p>State controllers and triggers. Pages in brackets describe how an engine version differs: '
                '1.1, win (WinMUGEN) and dos (DOS MUGEN).</p>\n'
                + body + '</body>\n</html>\n')
        with open(os.path.join(lang_dir, 'index.html'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(page)
    return 0


if __name__ == '__main__':
    sys.exit(main())
