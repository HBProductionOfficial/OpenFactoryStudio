#!/usr/bin/env python3
"""Writes data/docs/en-us/shortcuts.html. Run it after changing a default shortcut or caption."""
import html
import os
import re
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir)
CATALOGS = [os.path.join(ROOT, 'src', 'core', 'actions', 'ActionCatalog.cpp'),
            os.path.join(ROOT, 'src', 'engines', 'mugen', 'MugenActions.cpp')]
GROUPS = [('file_', 'Project'), ('edit_', 'Edit'), ('quick_search', 'Edit'), ('view_', 'View'),
          ('code_', 'Code'), ('debug_', 'Debug'), ('breakpoint_', 'Debug'), ('editor_', 'Editors'),
          ('def_', 'Definitions'), ('spr_', 'Sprites'), ('pal_', 'Palettes'), ('anim_', 'Animations'),
          ('bgs_', 'Backgrounds'), ('cmd_', 'Commands'), ('st_', 'States'), ('snd_', 'Sounds'),
          ('emul_', 'Emulator'), ('tools_', 'Tools'), ('ref_', 'Help'), ('help_', 'Help')]


def captions():
    found = {}
    entry = re.compile(r'\{\s*"([a-z0-9_]+)"\s*,\s*(?:QT_TRANSLATE_NOOP\(\s*"[^"]*"\s*,\s*)?"((?:[^"\\]|\\.)*)"')
    for path in CATALOGS:
        with open(path, encoding='utf-8') as f:
            for m in entry.finditer(f.read()):
                found.setdefault(m.group(1), m.group(2).replace('\\"', '"'))
    return found


def group_of(action):
    for prefix, name in GROUPS:
        if action.startswith(prefix):
            return name
    return 'Other'


def main():
    names = captions()
    tree = ET.parse(os.path.join(ROOT, 'data', 'shortcuts.xml'))
    grouped = {}
    for s in tree.getroot().iter('shortcut'):
        action = s.get('action')
        keys = s.get('keys', '')
        if not keys:
            continue
        caption = names.get(action, action).replace('&', '').rstrip('.').rstrip()
        grouped.setdefault(group_of(action), []).append((caption, keys, action))
    order = [name for _, name in GROUPS] + ['Other']
    sections = []
    for group in dict.fromkeys(order):
        rows = sorted(grouped.get(group, []), key=lambda r: r[0].lower())
        if not rows:
            continue
        body = '\n'.join('  <tr><td>%s</td><td><kbd>%s</kbd></td></tr>' % (html.escape(c), html.escape(k))
                         for c, k, _ in rows)
        sections.append('<h2>%s</h2>\n<table>\n  <tr><th>Action</th><th>Keys</th></tr>\n%s\n</table>\n'
                        % (group, body))
    page = ('<!DOCTYPE html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n<title>Keyboard shortcuts</title>\n'
            '<link rel="stylesheet" href="style.css">\n</head>\n<body>\n'
            '<nav class="top"><a href="index.html">Contents</a></nav>\n<h1>Keyboard shortcuts</h1>\n'
            '<p>These are the default keys. You can change them in <b>Tools &gt; Options &gt; Interface</b>. An '
            'action can have more than one key, and the same key can do different things in different editors.</p>\n'
            + ''.join(sections) + '</body>\n</html>\n')
    with open(os.path.join(ROOT, 'data', 'docs', 'en-us', 'shortcuts.html'), 'w', encoding='utf-8',
              newline='\n') as f:
        f.write(page)
    return 0


if __name__ == '__main__':
    sys.exit(main())
