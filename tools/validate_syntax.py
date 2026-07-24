#!/usr/bin/env python3
"""Checks the M.U.G.E.N syntax database (data/engines/mugen/syntax.xml) and the data made from it.

--corpus also lists what the database does not know in real M.U.G.E.N files. Exits with 1 on
errors (or unknowns with --strict) and 77 when --corpus-env finds no corpus, so CTest skips it.
"""
from __future__ import annotations

import argparse
import collections
import os
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DB = ROOT / 'data' / 'engines' / 'mugen' / 'syntax.xml'
SKIP_EXIT = 77

FILE_TYPES = ['char', 'state', 'command', 'stage', 'fight', 'select', 'motif', 'font', 'storyboard']
VERSIONS = {'WinBeta': 1, '1.0': 2, '1.1': 3, 'ext': 4}

# Parameter types and how many comma-separated components they take (None = variable).
PARAM_TYPES = {
    'int': 1, 'float': 1, 'string': 1, 'ustring': 1, 'file': 1, 'keyword': 1, 'keyseq': 1,
    'int2': 2, 'int3': 3, 'int4': 4, 'float2': 2, 'float3': 3, 'float4': 4, 'ifloat3': 4,
    'intvec': None, 'exp': 1, 'expi': 1, 'expf': 1, 'expb': 1,
    'exp2': 2, 'exp3': 3, 'exp4': 4, 'exp2i': 2, 'exp3i': 3, 'exp4i': 4,
    'exp2f': 2, 'exp3f': 3, 'exp4f': 4, 'exp2b': 2, 'exp3b': 3, 'expvec': None,
    'expkeyword': 2, 'exp2keyword': 3, 'hitattr': None, 'hitflags': 1, 'guardflags': 1,
}
KEYWORD_TYPES = {'keyword', 'expkeyword', 'exp2keyword'}
VECTOR_TYPES = {'intvec', 'expvec'}
TRIGGER_TYPES = {None, 'exp', 'exp2', 'exp3', 'keyword', 'opstr', 'opint', 'opint2', 'op2int2', 'valueeq',
                 'const', 'constexp', 'nilexp', 'expopt', 'hitattr'}
TRIGGER_LIST_TYPES = {'keyword', 'valueeq'}
TRIGGER_CONTEXTS = {'default', 'bgctrl', 'bg'}
REQUIRED_CONSTS = ['const', 'GetHitVar', 'AssertSpecial', 'StageVar']
CONST_TRIGGERS = {'const': 'const', 'gethitvar': 'GetHitVar', 'stagevar': 'StageVar',
                  'stageinfo': 'StageInfo'}
REDIRECTS = {'parent', 'root', 'helper', 'target', 'partner', 'enemy', 'enemynear', 'playerid'}
STATE_UNIVERSAL = re.compile(r'^(type|triggerall|trigger\d+|persistent|ignorehitpause)$', re.I)

BG_TYPES = ['normal', 'anim', 'parallax', 'dummy']
BGCTRL_TYPES = ['null', 'visible', 'enable', 'enabled', 'velset', 'veladd', 'posset', 'posadd', 'anim',
                'sinx', 'siny']


def wildcard_regex(pattern: str) -> re.Pattern:
    return re.compile('^' + '.*'.join(re.escape(p) for p in pattern.split('*')) + '$', re.I)


def specificity(pattern: str) -> int:
    return len(pattern.replace('*', ''))


class Param:
    def __init__(self, el: ET.Element):
        a = el.attrib
        self.name = a.get('name', '')
        self.type = a.get('type', '')
        self.required = a.get('required') == 'true'
        self.optional = [t.strip() for t in a['optional'].split(',')] if 'optional' in a else None
        self.max = a.get('max')
        self.list = [t.strip() for t in a['list'].split(',')] if 'list' in a else None
        self.char = a.get('char')
        self.format = a.get('format')
        self.nil = a.get('nil')
        self.minver = a.get('minver')
        self.deprecated = a.get('deprecated')
        self.attrib = a
        self.regex = wildcard_regex(self.name) if '*' in self.name else None


class Block:
    def __init__(self, el: ET.Element, ftype: str):
        full = el.get('type', '')
        full = full.replace('::', ':')
        name, _, base = full.partition(':')
        self.file = ftype
        self.name = name.strip()
        self.base = base.strip() or None
        self.minver = el.get('minver')
        self.deprecated = el.get('deprecated')
        self.raw = el.get('raw') == 'true'
        self.params = [Param(p) for p in el.findall('param')]
        if '>' in self.name:
            self.section, self.typename = (s.strip() for s in self.name.split('>', 1))
        else:
            self.section, self.typename = self.name, None
        self.section_words = self.section.split()
        self.section_regexes = [wildcard_regex(w) for w in self.section_words]
        self.resolved_base: Block | None = None


class Trigger:
    def __init__(self, el: ET.Element):
        a = el.attrib
        self.name = a.get('name', '')
        self.type = a.get('type')
        self.list = [t.strip() for t in a['list'].split(',')] if 'list' in a else None
        self.context = [t.strip() for t in a['context'].split(',')] if 'context' in a else None
        self.minver = a.get('minver')
        self.deprecated = a.get('deprecated')
        self.regex = wildcard_regex(self.name) if '*' in self.name else None


class SyntaxDB:
    def __init__(self, path: Path):
        self.path = path
        self.errors: list[str] = []
        self.files: dict[str, list[Block]] = {}
        self.consts: dict[str, list[str]] = {}
        self.triggers: list[Trigger] = []
        tree = ET.parse(path)
        root = tree.getroot()
        if root.tag != 'syntax':
            self.errors.append(f'root element is <{root.tag}>, expected <syntax>')
        for f in root.findall('file'):
            ftype = f.get('type', '')
            if ftype in self.files:
                self.errors.append(f'file type "{ftype}" is defined twice')
            self.files[ftype] = [Block(b, ftype) for b in f.findall('block')]
        for c in root.findall('consts'):
            name = c.get('name', '')
            if name in self.consts:
                self.errors.append(f'consts "{name}" is defined twice')
            self.consts[name] = [e.get('name', '') for e in c.findall('const')]
        self.triggers = [Trigger(t) for t in root.findall('trigger')]
        self.trigger_exact = {t.name.lower(): t for t in self.triggers if not t.regex}
        self.trigger_wild = sorted((t for t in self.triggers if t.regex), key=lambda t: -specificity(t.name))
        self.const_sets = {k.lower(): {e.lower() for e in v} for k, v in self.consts.items()}
        self._params_cache: dict[int, tuple[dict, list]] = {}
        self._resolve_bases()

    # -- inheritance ---------------------------------------------------------------------------
    def find_block(self, ftype: str, name: str, section: str | None = None) -> Block | None:
        blocks = self.files.get(ftype, [])
        low = name.lower()
        for b in blocks:
            if b.name.lower() == low:
                return b
        if section is not None:
            want = (section + '>' + name).lower()
            for b in blocks:
                if b.name.lower() == want:
                    return b
        for b in blocks:
            if b.typename and b.typename.lower() == low:
                return b
        return None

    def _builtin_alias(self, ref: str) -> Block | None:
        """Maps dotted references such as stage.BG.Normal or stage.BGCtrlDef.Visible."""
        parts = ref.split('.')
        if len(parts) < 2 or parts[0].lower() != 'stage':
            return None
        rest = [p.lower() for p in parts[1:]]
        target = None
        if len(rest) == 1:
            if rest[0] in ('bgdef', 'bgctrldef'):
                target = rest[0]
            elif rest[0] in BG_TYPES:
                target = 'bg>' + rest[0]
            elif rest[0] in BGCTRL_TYPES:
                target = 'bgctrl>' + rest[0]
        elif len(rest) == 2 and rest[0] == 'bg' and rest[1] in BG_TYPES:
            target = 'bg>' + rest[1]
        elif len(rest) == 2 and rest[0] in ('bgctrldef', 'bgctrl') and rest[1] in BGCTRL_TYPES:
            target = 'bgctrl>' + rest[1]
        if target is None:
            return None
        return self.find_block('stage', target)

    def resolve(self, block: Block) -> Block | None:
        ref = block.base
        if ref is None:
            return None
        if '.' in ref:
            ftype, _, name = ref.partition('.')
            if ftype in self.files:
                found = self.find_block(ftype, name, block.section)
                if found is not None:
                    return found
            return self._builtin_alias(ref)
        return self.find_block(block.file, ref, block.section)

    def _resolve_bases(self):
        for ftype, blocks in self.files.items():
            for b in blocks:
                if b.base:
                    b.resolved_base = self.resolve(b)

    def block_params(self, block: Block) -> tuple[dict[str, Param], list[Param]]:
        """All parameters of a block. Its own ones override inherited ones of the same name."""
        key = id(block)
        if key in self._params_cache:
            return self._params_cache[key]
        exact: dict[str, Param] = {}
        wild: dict[str, Param] = {}
        chain = []
        seen = set()
        b = block
        while b is not None and id(b) not in seen:
            seen.add(id(b))
            chain.append(b)
            b = b.resolved_base
        for b in reversed(chain):
            for p in b.params:
                (wild if p.regex else exact)[p.name.lower()] = p
        result = (exact, sorted(wild.values(), key=lambda p: -specificity(p.name)))
        self._params_cache[key] = result
        return result

    def find_param(self, block: Block, key: str) -> Param | None:
        exact, wild = self.block_params(block)
        p = exact.get(key.lower())
        if p is not None:
            return p
        for p in wild:
            if p.regex.match(key):
                return p
        return None

    def find_trigger(self, name: str) -> Trigger | None:
        t = self.trigger_exact.get(name.lower())
        if t is not None:
            return t
        for t in self.trigger_wild:
            if t.regex.match(name):
                return t
        return None

    # -- section matching ----------------------------------------------------------------------
    def match_section(self, ftype: str, header: str):
        """Returns (kind, payload): ('plain', block), ('typed', [family blocks]) or (None, None)."""
        result = self._match(ftype, header)
        if result[0] is None:
            m = re.match(r'^[A-Za-z]{2}\.(.+)$', header.strip())
            if m:
                result = self._match(ftype, m.group(1))
        return result

    def _match(self, ftype: str, header: str):
        words = header.split()
        if not words:
            return None, None
        best = None
        families: dict[str, list[Block]] = collections.defaultdict(list)
        for b in self.files.get(ftype, []):
            if b.typename is not None:
                if len(b.section_words) == 1 and b.section_regexes[0].match(words[0]):
                    families[b.section.lower()].append(b)
            else:
                n = len(b.section_words)
                if len(words) >= n and all(r.match(w) for r, w in zip(b.section_regexes, words)):
                    spec = specificity(b.section)
                    if best is None or spec > best[0]:
                        best = (spec, 'plain', b)
        for fam, blocks in families.items():
            spec = specificity(blocks[0].section)
            if best is None or spec > best[0]:
                best = (spec, 'typed', blocks)
        if best is None:
            return None, None
        return best[1], best[2]


# ============================================================================== consistency
def check_db(db: SyntaxDB) -> list[str]:
    errors = list(db.errors)
    for ftype in db.files:
        if ftype not in FILE_TYPES:
            errors.append(f'unknown file type "{ftype}"')
    for ftype in FILE_TYPES:
        if ftype not in db.files:
            errors.append(f'file type "{ftype}" is missing')

    def check_version(where, attr, value):
        if value is not None and value not in VERSIONS:
            errors.append(f'{where}: {attr}="{value}" is not a version token')

    for ftype, blocks in db.files.items():
        names = collections.Counter(b.name.lower() for b in blocks)
        for n, c in names.items():
            if c > 1:
                errors.append(f'{ftype}: block "{n}" is defined {c} times')
        for b in blocks:
            where = f'{ftype}: block "{b.name}"'
            if not b.name:
                errors.append(f'{ftype}: block without a name')
            if b.typename is not None and (not b.section or not b.typename):
                errors.append(f'{where}: malformed typed block name')
            if b.base and b.resolved_base is None:
                errors.append(f'{where}: inherits "{b.base}", which does not exist')
            seen = set()
            cur = b
            while cur is not None:
                if id(cur) in seen:
                    errors.append(f'{where}: inheritance cycle')
                    break
                seen.add(id(cur))
                cur = cur.resolved_base
            check_version(where, 'minver', b.minver)
            check_version(where, 'deprecated', b.deprecated)
            if b.minver and b.deprecated and VERSIONS.get(b.minver, 0) >= VERSIONS.get(b.deprecated, 9):
                errors.append(f'{where}: deprecated is not newer than minver')
            if b.raw and b.params:
                errors.append(f'{where}: a raw block cannot have parameters')
            pnames = collections.Counter(p.name.lower() for p in b.params)
            for n, c in pnames.items():
                if c > 1:
                    errors.append(f'{where}: parameter "{n}" is defined {c} times')
            for p in b.params:
                errors += check_param(f'{where}, param "{p.name}"', p)
    tnames = collections.Counter(t.name.lower() for t in db.triggers)
    for n, c in tnames.items():
        if c > 1:
            errors.append(f'trigger "{n}" is defined {c} times')
    for t in db.triggers:
        where = f'trigger "{t.name}"'
        if not re.match(r'^[A-Za-z][A-Za-z0-9_]*\*?$', t.name):
            errors.append(f'{where}: invalid name')
        if t.type not in TRIGGER_TYPES:
            errors.append(f'{where}: unknown type "{t.type}"')
        if t.type in TRIGGER_LIST_TYPES and not t.list:
            errors.append(f'{where}: type "{t.type}" needs a list')
        if t.context:
            for c in t.context:
                if c not in TRIGGER_CONTEXTS:
                    errors.append(f'{where}: unknown context "{c}"')
        check_version(where, 'minver', t.minver)
        check_version(where, 'deprecated', t.deprecated)
        if t.type in ('const', 'constexp'):
            cname = CONST_TRIGGERS.get(t.name.lower())
            if cname is None or cname.lower() not in db.const_sets:
                errors.append(f'{where}: no <consts> list for this trigger')
    for name in REQUIRED_CONSTS:
        if name not in db.consts:
            errors.append(f'consts "{name}" is missing')
    for name, entries in db.consts.items():
        c = collections.Counter(e.lower() for e in entries)
        for e, n in c.items():
            if n > 1:
                errors.append(f'consts "{name}": "{e}" is listed {n} times')
            if not e:
                errors.append(f'consts "{name}": empty entry')
    return errors


def check_param(where: str, p: Param) -> list[str]:
    errors = []
    if not p.name:
        errors.append(f'{where}: no name')
    elif not re.match(r'^[A-Za-z0-9_.*]+$', p.name):
        errors.append(f'{where}: invalid name')
    if p.type not in PARAM_TYPES:
        errors.append(f'{where}: unknown type "{p.type}"')
        return errors
    if p.type in KEYWORD_TYPES and not p.list:
        errors.append(f'{where}: keyword type without a list')
    if p.list is not None:
        if any(not k for k in p.list):
            errors.append(f'{where}: empty keyword in list')
        if len({k.lower() for k in p.list}) != len(p.list):
            errors.append(f'{where}: duplicate keyword in list')
    if p.max is not None:
        if p.type not in VECTOR_TYPES:
            errors.append(f'{where}: max is only valid for intvec/expvec')
        elif not p.max.isdigit() or int(p.max) < 1:
            errors.append(f'{where}: invalid max "{p.max}"')
    elif p.type == 'intvec':
        errors.append(f'{where}: intvec needs max')
    if p.optional is not None:
        if any(t not in ('req', 'def', 'prev') for t in p.optional):
            errors.append(f'{where}: invalid optional pattern "{",".join(p.optional)}"')
        n = PARAM_TYPES[p.type]
        if n is None and p.max is not None and p.max.isdigit():
            n = int(p.max)
        if n is not None and len(p.optional) > n:
            errors.append(f'{where}: optional pattern has more entries than components')
        if p.optional and p.optional[0] == 'prev':
            errors.append(f'{where}: the first component cannot be "prev"')
    if p.char is not None and (not p.char or any(c not in 'SF' for c in p.char)):
        errors.append(f'{where}: invalid char "{p.char}"')
    if p.format is not None and p.format != 'indexed':
        errors.append(f'{where}: invalid format "{p.format}"')
    for flag in ('required', 'nil'):
        v = p.attrib.get(flag)
        if v is not None and v != 'true':
            errors.append(f'{where}: {flag}="{v}" (only "true" is allowed)')
    for attr in ('minver', 'deprecated'):
        v = p.attrib.get(attr)
        if v is not None and v not in VERSIONS:
            errors.append(f'{where}: {attr}="{v}" is not a version token')
    if p.minver and p.deprecated and VERSIONS.get(p.minver, 0) >= VERSIONS.get(p.deprecated, 9):
        errors.append(f'{where}: deprecated is not newer than minver')
    known = {'name', 'type', 'required', 'optional', 'max', 'list', 'char', 'format', 'nil', 'minver',
             'deprecated'}
    for attr in p.attrib:
        if attr not in known:
            errors.append(f'{where}: unknown attribute "{attr}"')
    return errors


# ============================================================================== derived data
def controller_names(db: SyntaxDB) -> list[str]:
    return [b.typename for b in db.files.get('state', []) if b.section.lower() == 'state' and b.typename]


def check_data_dir(db: SyntaxDB, data_dir: Path) -> list[str]:
    errors = []
    names = controller_names(db)
    comp = data_dir / 'completer.xml'
    if comp.is_file():
        root = ET.parse(comp).getroot()
        if root.tag != 'completion':
            errors.append('completer.xml: root element must be <completion>')
        models = {m.get('prefix', ''): [w.get('value', '') for w in m.findall('word')] for m in root.findall('model')}
        for prefix, words in models.items():
            if len(set(words)) != len(words):
                errors.append(f'completer.xml: model "{prefix}" has duplicate words')
        types = models.get('type=')
        if types is None:
            errors.append('completer.xml: the "type=" model is missing')
        elif sorted(w.strip().lower() for w in types) != sorted(n.lower() for n in names):
            missing = {n.lower() for n in names} - {w.strip().lower() for w in types}
            extra = {w.strip().lower() for w in types} - {n.lower() for n in names}
            errors.append(f'completer.xml: "type=" does not match the controllers (missing {sorted(missing)}, '
                          f'extra {sorted(extra)})')
        consts = models.get('const(')
        if consts is None:
            errors.append('completer.xml: the "const(" model is missing')
        else:
            want = {c.lower() + ')' for c in db.consts.get('const', [])}
            if {w.lower() for w in consts} != want:
                errors.append('completer.xml: "const(" does not match the const list')
        for prefix, words in models.items():
            if prefix.lower() == 'gethitvar(':
                want = {c.lower() + ')' for c in db.consts.get('GetHitVar', [])}
                if {w.lower() for w in words} != want:
                    errors.append('completer.xml: "gethitvar(" does not match the GetHitVar list')
    sdir = data_dir / 'sctrls'
    if sdir.is_dir():
        have = {p.stem.lower(): p for p in sdir.glob('*.sctrl')}
        for n in names + ['StateDef']:
            if n.lower() not in have:
                errors.append(f'sctrls: no template for {n}')
        for stem, p in have.items():
            data = p.read_bytes()
            if b'\n' in data.replace(b'\r\n', b''):
                errors.append(f'sctrls/{p.name}: line endings must be CRLF')
            try:
                data.decode('ascii')
            except UnicodeDecodeError:
                errors.append(f'sctrls/{p.name}: must be plain ASCII')
    ndir = data_dir / 'snippets'
    if ndir.is_dir():
        for p in sorted(ndir.glob('*.xml')):
            errors += check_snippet(p)
    pdir = data_dir / 'palFX'
    if pdir.is_dir():
        for p in sorted(pdir.glob('*.xml')):
            errors += check_palfx(p)
    ddir = data_dir / 'docs'
    if ddir.is_dir():
        for lang in sorted(d for d in ddir.iterdir() if d.is_dir()):
            common = lang / 'common'
            for n in names:
                if not (common / f'sctrl.{n.lower()}.html').is_file():
                    errors.append(f'docs/{lang.name}/common: no page for controller {n}')
            for t in db.triggers:
                if t.regex:
                    continue
                if not (common / f'trigger.{t.name.lower()}.html').is_file():
                    errors.append(f'docs/{lang.name}/common: no page for trigger {t.name}')
            for variant in lang.iterdir():
                if variant.is_dir() and variant.name not in ('common', '1.0', '1.1', 'dos', 'win'):
                    errors.append(f'docs/{lang.name}: unknown variant folder "{variant.name}"')
                if variant.is_dir():
                    for page in variant.iterdir():
                        if not re.match(r'^(sctrl|trigger)\.[a-z0-9_]+\.html$', page.name):
                            errors.append(f'docs/{lang.name}/{variant.name}: unexpected file "{page.name}"')
    return errors


def check_snippet(p: Path) -> list[str]:
    errors = []
    try:
        root = ET.parse(p).getroot()
    except ET.ParseError as e:
        return [f'snippets/{p.name}: {e}']
    if root.tag != 'snippet':
        errors.append(f'snippets/{p.name}: root element must be <snippet>')
    code = root.find('code')
    if code is None or not (code.text or '').strip():
        errors.append(f'snippets/{p.name}: no <code>')
        return errors
    text = code.text
    for f in root.findall('field'):
        name, ftype = f.get('name', ''), f.get('type', '')
        if not name or not f.get('title'):
            errors.append(f'snippets/{p.name}: field without name or title')
        if ftype not in ('int', 'string', 'items'):
            errors.append(f'snippets/{p.name}: field "{name}" has unknown type "{ftype}"')
        if ftype == 'items':
            values = (f.get('value') or '').split(',')
            titles = (f.get('value_title') or '').split(',')
            if not f.get('value') or len(values) != len(titles):
                errors.append(f'snippets/{p.name}: field "{name}" needs value and value_title of equal length')
        if '@' + name not in text:
            errors.append(f'snippets/{p.name}: field "{name}" is not used in the code')
    return errors


def check_palfx(p: Path) -> list[str]:
    errors = []
    try:
        root = ET.parse(p).getroot()
    except ET.ParseError as e:
        return [f'palFX/{p.name}: {e}']
    if root.tag != 'palfx':
        return [f'palFX/{p.name}: root element must be <palfx>']

    def ints(el, attrs, lo, hi):
        if el is None:
            errors.append(f'palFX/{p.name}: missing <{attrs[0]}>')
            return
        for a in attrs[1:]:
            v = el.get(a)
            if v is None or not re.match(r'^-?\d+$', v) or not lo <= int(v) <= hi:
                errors.append(f'palFX/{p.name}: <{el.tag} {a}="{v}"> out of range {lo}..{hi}')

    ints(root.find('add'), ['add', 'red', 'green', 'blue'], -255, 255)
    ints(root.find('mul'), ['mul', 'red', 'green', 'blue'], 0, 1024)
    ints(root.find('sin'), ['sin', 'red', 'green', 'blue'], -255, 255)
    sin = root.find('sin')
    if sin is not None:
        ints(sin, ['sin', 'period'], 0, 10000)
    ints(root.find('color'), ['color', 'value'], 0, 256)
    inv = root.find('invert')
    if inv is None or inv.get('value') not in ('true', 'false'):
        errors.append(f'palFX/{p.name}: <invert value> must be true or false')
    return errors


# ============================================================================== corpus
class Section:
    def __init__(self, header: str, line: int):
        self.header = header
        self.line = line
        self.items: list[tuple[str, str, int]] = []

    def get(self, key: str) -> str | None:
        for k, v, _ in self.items:
            if k.lower() == key:
                return v
        return None


def strip_comment(line: str) -> str:
    out = []
    quoted = False
    for ch in line:
        if ch == '"':
            quoted = not quoted
        elif ch == ';' and not quoted:
            break
        out.append(ch)
    return ''.join(out)


def read_text(path: Path) -> str:
    data = path.read_bytes()
    if data.startswith(b'\xef\xbb\xbf'):
        data = data[3:]
    try:
        return data.decode('utf-8')
    except UnicodeDecodeError:
        return data.decode('latin-1')


def parse_ini(path: Path) -> list[Section]:
    sections: list[Section] = []
    cur = None
    for n, raw in enumerate(read_text(path).splitlines(), 1):
        line = strip_comment(raw).strip()
        if not line:
            continue
        if line.startswith('['):
            end = line.find(']')
            header = line[1:end if end > 0 else len(line)].strip()
            cur = Section(header, n)
            sections.append(cur)
            continue
        if cur is None:
            continue
        if '=' in line:
            k, v = line.split('=', 1)
            cur.items.append((k.strip(), v.strip(), n))
        else:
            cur.items.append(('', line, n))
    return sections


def detect_type(path: Path, sections: list[Section]) -> str | None:
    ext = path.suffix.lower()
    if ext in ('.cns', '.st'):
        return 'state'
    if ext == '.cmd':
        return 'command'
    if ext != '.def':
        return None
    heads = {re.sub(r'^[a-z]{2}\.', '', s.header.lower()).split()[0] for s in sections if s.header.split()}
    full = {s.header.lower() for s in sections}
    files = next((s for s in sections if s.header.lower() == 'files'), None)
    if 'fnt' in heads or 'fnt v2' in full:
        return 'font'
    if 'scenedef' in heads:
        return 'storyboard'
    if 'characters' in heads or 'extrastages' in heads:
        return 'select'
    if 'lifebar' in heads or 'powerbar' in heads or 'round' in heads or 'winicon' in heads:
        return 'fight'
    if 'title info' in full or 'select info' in full or (
            files is not None and (files.get('fight') is not None or files.get('select') is not None)):
        return 'motif'
    if 'camera' in heads or 'stageinfo' in heads or 'playerinfo' in heads or 'bgdef' in heads:
        return 'stage'
    if files is not None and (files.get('cns') is not None or files.get('sprite') is not None):
        return 'char'
    return None


TOKEN = re.compile(r'\s*(?:(?P<str>"[^"]*"?)|(?P<num>\d+\.?\d*|\.\d+)|(?P<id>[A-Za-z_][A-Za-z0-9_.]*)|'
                   r'(?P<op>:=|==|!=|<=|>=|&&|\|\||\^\^|\*\*|[-+*/%!~&|^=<>(),\[\]]))')


def tokenize(expr: str) -> list[tuple[str, str]]:
    toks = []
    pos = 0
    while pos < len(expr):
        m = TOKEN.match(expr, pos)
        if not m or m.end() == pos:
            if expr[pos].isspace():
                pos += 1
                continue
            toks.append(('bad', expr[pos]))
            pos += 1
            continue
        pos = m.end()
        kind = m.lastgroup
        if kind:
            toks.append((kind, m.group(kind)))
    return toks


class Report:
    def __init__(self):
        self.files = collections.Counter()
        self.undetected: list[str] = []
        # kind -> file type -> name -> [count, first location]
        self.unknown: dict[str, dict[str, dict[str, list]]] = collections.defaultdict(
            lambda: collections.defaultdict(dict))

    def add(self, kind: str, ftype: str, name: str, where: str):
        entry = self.unknown[kind][ftype].setdefault(name, [0, where])
        entry[0] += 1

    def total(self) -> int:
        return sum(len(names) for byfile in self.unknown.values() for names in byfile.values())


def scan_expression(db: SyntaxDB, expr: str, report: Report, ftype: str, where: str, param: Param | None):
    toks = tokenize(expr)
    allowed = {k.lower() for k in param.list} if param is not None and param.list else set()
    char_ok = param is not None and param.char
    i = 0
    n = len(toks)
    while i < n:
        kind, val = toks[i]
        if kind != 'id':
            i += 1
            continue
        low = val.lower()
        trig = db.find_trigger(val) if '.' not in val else None
        if trig is not None:
            t = trig.type
            if t == 'keyword' and i + 1 < n and toks[i + 1][0] == 'id':
                i += 1
            elif t in ('const', 'constexp') and i + 2 < n and toks[i + 1][1] == '(' and toks[i + 2][0] == 'id':
                cname = CONST_TRIGGERS.get(trig.name.lower())
                if cname and cname.lower() in db.const_sets and toks[i + 2][1].lower() not in db.const_sets[cname.lower()]:
                    report.add('constants', ftype, f'{trig.name}({toks[i + 2][1]})', where)
                i += 2
            elif t == 'valueeq' and i + 2 < n and toks[i + 1][1] in ('=', '!=') and toks[i + 2][0] == 'id':
                i += 2
            elif t == 'hitattr':
                depth = 0
                j = i + 1
                while j < n:
                    k2, v2 = toks[j]
                    if v2 == '(':
                        depth += 1
                    elif v2 == ')':
                        if depth == 0:
                            break
                        depth -= 1
                    elif v2 in ('&&', '||', '^^') and depth == 0:
                        break
                    j += 1
                i = j - 1
        elif low in REDIRECTS or low in allowed:
            pass
        elif char_ok and re.match(r'^[SsFf]\d+$', val):
            pass
        elif char_ok and val.upper() in param.char and (i == 0 or toks[i - 1][1] == ','):
            pass  # prefix letter before a parenthesised expression, e.g. anim = F(60 + 1)
        else:
            report.add('triggers', ftype, val, where)
        i += 1


def match_type(family: list[Block], value: str) -> Block | None:
    """The engine reads only the first word of a `type` value. Outside state controllers it also
    takes a unique prefix (`type = norm` for a normal background element)."""
    low = value.strip().lower()
    for cand in (low, low.split()[0] if low.split() else low):
        block = next((b for b in family if b.typename.lower() == cand), None)
        if block is not None:
            return block
    if family and family[0].section.lower() != 'state' and low:
        hits = [b for b in family if b.typename.lower().startswith(low.split()[0])]
        if len(hits) == 1:
            return hits[0]
    return None


def check_file(db: SyntaxDB, path: Path, report: Report, forced: str | None = None):
    try:
        sections = parse_ini(path)
    except OSError as e:
        report.add('files', 'unreadable', str(path), str(e))
        return
    ftype = forced or detect_type(path, sections)
    if ftype is None:
        if path.suffix.lower() == '.def':
            report.undetected.append(str(path))
        return
    report.files[ftype] += 1
    bg_names = []
    if ftype == 'storyboard':
        # Scenes name their background with bg.name. Its <name>* sections are checked as BG* blocks.
        bg_names = sorted({v.strip() for s in sections for k, v, _ in s.items
                           if k.lower() == 'bg.name' and v.strip()}, key=len, reverse=True)
    for sec in sections:
        header = sec.header
        where = f'{path}:{sec.line}'
        if re.match(r'(?i)^begin\s+action\b', header):
            continue  # animation data (AIR format)
        for bgname in bg_names:
            if header.lower().startswith(bgname.lower()):
                header = 'BG' + header[len(bgname):]
                break
        kind, payload = db.match_section(ftype, header)
        if kind is None:
            report.add('sections', ftype, '[' + re.sub(r'\s+\S.*$', ' ...', header) + ']'
                       if ftype in ('state', 'command') else '[' + header + ']', where)
            continue
        block = None
        if kind == 'plain':
            block = payload
        else:
            tval = sec.get('type')
            fam = payload[0].section
            if tval is None:
                report.add('controllers', ftype, f'{fam} (no type)', where)
                continue
            block = match_type(payload, tval)
            if block is None:
                report.add('controllers', ftype, f'{fam}>{tval.strip()}', where)
                continue
        if block.raw:
            continue
        is_state = ftype in ('state', 'command') and block.section.lower() == 'state'
        for key, value, line in sec.items:
            loc = f'{path}:{line}'
            if not key:
                continue
            lk = key.lower()
            if kind == 'typed' and lk == 'type':
                continue
            if is_state and STATE_UNIVERSAL.match(lk):
                if lk.startswith('trigger'):
                    scan_expression(db, value, report, ftype, loc, None)
                continue
            m = re.match(r'^([A-Za-z_][\w.]*)\s*\((.*)\)$', key)
            if m:
                p = db.find_param(block, m.group(1))
                if p is None or (p.format != 'indexed' and p.name != '*'):
                    report.add('params', ftype, f'{block.name}: {m.group(1)}(n)', loc)
                    continue
            else:
                p = db.find_param(block, key)
                if p is None:
                    report.add('params', ftype, f'{block.name}: {key.lower()}', loc)
                    continue
            if ftype in ('state', 'command') and p.type.startswith('exp'):
                scan_expression(db, value, report, ftype, loc, p)


CORPUS_EXT = ('.def', '.cns', '.st', '.cmd')


def corpus_files(paths: list[Path], recurse: bool = True) -> list[Path]:
    out = []
    for p in paths:
        if p.is_file():
            out.append(p)
        elif p.is_dir():
            it = p.rglob('*') if recurse else p.glob('*')
            out += sorted(f for f in it if f.is_file() and f.suffix.lower() in CORPUS_EXT)
    return out


def print_report(title: str, report: Report, out=sys.stdout):
    print(f'== {title} ==', file=out)
    print('files: ' + (', '.join(f'{t} {n}' for t, n in sorted(report.files.items())) or 'none'), file=out)
    for u in report.undetected:
        print(f'  not recognised (skipped): {u}', file=out)
    for kind in ('sections', 'controllers', 'params', 'triggers', 'constants', 'files'):
        byfile = report.unknown.get(kind, {})
        for ftype in sorted(byfile):
            names = byfile[ftype]
            print(f'unknown {kind} ({ftype}): {len(names)}', file=out)
            for name, (count, where) in sorted(names.items(), key=lambda kv: kv[0].lower()):
                print(f'    {name}  x{count}  (first: {where})', file=out)
    print(f'total unknowns: {report.total()}', file=out)


def run_corpus(db: SyntaxDB, title: str, paths: list[Path], recurse: bool = True) -> Report:
    report = Report()
    for f in corpus_files(paths, recurse):
        check_file(db, f, report)
    print_report(title, report)
    return report


def env_corpora() -> list[tuple[str, list[Path], bool]]:
    out = []
    kfm = os.environ.get('OFS_CORPUS_KFM')
    if kfm:
        out.append(('KFM (' + kfm + ')', [Path(kfm)], True))
    mugen = os.environ.get('OFS_CORPUS_MUGEN')
    if mugen:
        root = Path(mugen)
        data = root / 'data'
        files = [f for f in sorted(data.glob('*')) if f.is_file() and f.suffix.lower() in CORPUS_EXT]
        out.append(('M.U.G.E.N data (' + str(data) + ')', files, False))
        out.append(('M.U.G.E.N stages (' + str(root / 'stages') + ')', [root / 'stages'], False))
        out.append(('M.U.G.E.N fonts (' + str(root / 'font') + ')', [root / 'font'], False))
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--syntax', type=Path, default=DEFAULT_DB, help='syntax.xml to check')
    ap.add_argument('--no-data', action='store_true', help='skip the checks of the derived engine data')
    ap.add_argument('--corpus', nargs='+', type=Path, metavar='PATH', help='files or folders to check')
    ap.add_argument('--corpus-env', action='store_true',
                    help='check the corpora named by OFS_CORPUS_KFM / OFS_CORPUS_MUGEN')
    ap.add_argument('--strict', action='store_true', help='fail when a corpus has unknowns')
    args = ap.parse_args(argv)
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(errors='backslashreplace')

    db = SyntaxDB(args.syntax)
    errors = check_db(db)
    if not args.no_data:
        errors += check_data_dir(db, args.syntax.parent)
    for e in errors:
        print('error: ' + e)
    blocks = sum(len(b) for b in db.files.values())
    params = sum(len(x.params) for b in db.files.values() for x in b)
    print(f'{args.syntax.name}: {len(db.files)} file types, {blocks} blocks, {params} parameters, '
          f'{len(controller_names(db))} controllers, {len(db.triggers)} triggers, '
          f'{sum(len(v) for v in db.consts.values())} constants: '
          + ('OK' if not errors else f'{len(errors)} error(s)'))
    if errors:
        return 1

    corpora = []
    if args.corpus:
        corpora.append(('corpus', args.corpus, True))
    if args.corpus_env:
        found = env_corpora()
        if not found and not args.corpus:
            print('OFS_CORPUS_KFM / OFS_CORPUS_MUGEN are not set: corpus check skipped')
            return SKIP_EXIT
        corpora += found
    unknowns = 0
    for title, paths, recurse in corpora:
        unknowns += run_corpus(db, title, paths, recurse).total()
    if args.strict and unknowns:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
