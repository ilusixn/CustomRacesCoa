"""Compare what CoA64 registers against the Extensions.dll reconstruction's inventory.

Usage:
    python Tools/coa/inventory.py --recon F:/WorkDir/ascension-extensions-reconstruction --out CoA/inventory-report.md

The reference side is parsed from the reconstruction's generated `docs/DLL_REFERENCE.md`. The CoA side is read
from the registration macros in `CoA/Source` (`COA_NATIVE`, `COA_PACKET`, `COA_CVAR`, `COA_EVENT`) and from the
`// CoA: ... (orig hook 0xADDR)` markers anywhere in the tree.
"""
import argparse
import os
import re
import sys
from dataclasses import dataclass, field

ROW_RE = re.compile(r'^\|\s*`([^`]+)`\s*\|\s*([^|]*?)\s*\|')
NATIVE_RE = re.compile(r'COA_NATIVE\(\s*"([^"]*)"\s*,\s*"([^"]+)"')
PACKET_RE = re.compile(r'COA_PACKET\(\s*(0x[0-9A-Fa-f]+|\d+)')
CVAR_RE = re.compile(r'COA_CVAR\(\s*"([^"]+)"')
EVENT_RE = re.compile(r'COA_EVENT\(\s*"([^"]+)"')
HOOK_RE = re.compile(r'//\s*CoA:.*?orig hook ((?:0x[0-9A-Fa-f]+[/, ]*)+)')
PATCH_RE = re.compile(r'//\s*CoA:.*?Extensions\.dll patch ((?:0x[0-9A-Fa-f]+[/, ]*)+)')
PATCH_ROW_RE = re.compile(r'^\s*\{(0x[0-9A-Fa-f]+)\s*,', re.M)
PATCH_TABLES = ('AscAttachPatches.generated.inc', 'AscAttachLate.generated.inc', 'AscAttachInline.generated.inc')
SOURCE_EXT = ('.c', '.cpp', '.h', '.hpp', '.inc')
SKIP_DIRS = {'.git', 'out', 'build', 'ThirdParty', 'bin', '.superpowers'}


@dataclass
class Inventory:
    natives: set = field(default_factory=set)   # (namespace, name, state)
    packets: set = field(default_factory=set)   # server -> client opcodes handled
    cvars: set = field(default_factory=set)
    events: set = field(default_factory=set)
    hooks: set = field(default_factory=set)     # client addresses detoured / replaced / jumped
    patches: set = field(default_factory=set)   # client addresses the DLL byte-patches at attach


def parse_reference(path):
    inv = Inventory()
    section = None
    namespace = None
    table = None
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if line.startswith('## '):
                section = line[3:].strip()
                table = None
                continue
            if line.startswith('### '):
                namespace = line[4:].strip()
                table = None
                continue
            if line.startswith('| ') and not line.startswith('| `'):
                table = line.split('|')[1].strip()
                continue
            m = ROW_RE.match(line)
            if not m:
                continue
            key, second = m.group(1), m.group(2)
            if section == 'Lua API index' and table == 'Name':
                inv.natives.add(('' if namespace == '(global)' else namespace, key, second.strip()))
            elif section == 'Server packet index (SMSG)' and table == 'Opcode':
                inv.packets.add(int(key, 16))
            elif section == 'Event index' and table == 'Event':
                inv.events.add(key)
            elif section == 'Modules' and table == 'Address':
                inv.hooks.add(int(key, 16))
            elif section == 'Modules' and table == 'CVar':
                inv.cvars.add(key)
    return inv


def parse_patches(recon_root):
    patches = set()
    for name in PATCH_TABLES:
        with open(os.path.join(recon_root, 'src', 'Ascension', name), encoding='utf-8') as f:
            patches.update(int(a, 16) for a in PATCH_ROW_RE.findall(f.read()))
    return patches


def scan_coa(root):
    inv = Inventory()
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        in_coa = os.sep + os.path.join('CoA', 'Source') in dirpath + os.sep or dirpath.replace('/', os.sep).endswith(os.path.join('CoA', 'Source'))
        for name in filenames:
            if not name.endswith(SOURCE_EXT):
                continue
            with open(os.path.join(dirpath, name), encoding='utf-8', errors='ignore') as f:
                text = f.read()
            for m in HOOK_RE.finditer(text):
                for addr in re.findall(r'0x[0-9A-Fa-f]+', m.group(1)):
                    inv.hooks.add(int(addr, 16))
            for m in PATCH_RE.finditer(text):
                for addr in re.findall(r'0x[0-9A-Fa-f]+', m.group(1)):
                    inv.patches.add(int(addr, 16))
            if not in_coa:
                continue
            # The state is not declared per native; it comes from BindingStates.generated.inc.
            for ns, fn in NATIVE_RE.findall(text):
                inv.natives.add((ns, fn, ''))
            inv.packets.update(int(op, 0) for op in PACKET_RE.findall(text))
            inv.cvars.update(CVAR_RE.findall(text))
            inv.events.update(EVENT_RE.findall(text))
    return inv


def report(ref, have):
    def pairs(natives):
        return {(ns, name) for ns, name, _ in natives}

    rows = [
        ('Lua natives (namespace, name)', pairs(ref.natives), pairs(have.natives),
         lambda k: f'{k[0]}.{k[1]}' if k[0] else k[1]),
        ('Server packets', ref.packets, have.packets, lambda k: f'0x{k:03X}'),
        ('Client hooks', ref.hooks, have.hooks, lambda k: f'0x{k:06X}'),
        ('Client byte patches', ref.patches, have.patches, lambda k: f'0x{k:06X}'),
        ('CVars', ref.cvars, have.cvars, str),
        ('Events', ref.events, have.events, str),
    ]
    out = ['# Extensions.dll port coverage', '', 'Generated by `Tools/coa/inventory.py`. Do not edit by hand.', '',
           '| Category | Ported | Total |', '|---|---:|---:|']
    for name, total, got, _ in rows:
        out.append(f'| {name} | {len(total & got)} | {len(total)} |')
    for name, total, got, fmt in rows:
        missing = sorted(total - got)
        out += ['', f'## Missing: {name} ({len(missing)})', '']
        out += [f'- `{fmt(k)}`' for k in missing]
    return '\n'.join(out) + '\n'


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--recon', required=True)
    p.add_argument('--root', default='.')
    p.add_argument('--out', default='CoA/inventory-report.md')
    args = p.parse_args(argv)
    ref = parse_reference(os.path.join(args.recon, 'docs', 'DLL_REFERENCE.md'))
    ref.patches = parse_patches(args.recon)
    text = report(ref, scan_coa(args.root))
    with open(args.out, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    print('\n'.join(text.splitlines()[4:12]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
