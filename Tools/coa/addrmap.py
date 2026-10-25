"""Map 32-bit client addresses used by the Extensions.dll reconstruction to symbols in this source tree.

Usage:
    python Tools/coa/addrmap.py --decomp . --recon F:/WorkDir/ascension-extensions-reconstruction --out CoA/addrmap.tsv

Every function in the decomp carries `// OFFSET: 0xADDR` above it; that gives the address -> symbol index.
Every client address literal in the reconstruction's sources is listed with where it is used and whether the
decomp has it. Globals and data have no OFFSET tag; they are resolved by hand in `CoA/addrmap.manual.tsv`
(same columns), which is merged in and never overwritten.
"""
import argparse
import os
import re
import sys
from dataclasses import dataclass, field

CLIENT_LO = 0x00401000
CLIENT_HI = 0x00DFFFFF
COLUMNS = ['addr', 'kind', 'recon_refs', 'decomp_symbol', 'decomp_file:line', 'status']

OFFSET_RE = re.compile(r'//\s*OFFSET:\s*0x([0-9A-Fa-f]+)')
SYMBOL_RE = re.compile(r'([A-Za-z_~][\w:~]*)\s*\(')
ADDR_RE = re.compile(r'(?<![\w.])0x([0-9A-Fa-f]{6,8})\b')
SOURCE_EXT = ('.c', '.cpp', '.h', '.hpp', '.inc')
SKIP_DIRS = {'.git', 'out', 'build', 'ThirdParty', 'third_party', 'bin', '.superpowers'}
FUNC_HINT_RE = re.compile(r'(__cdecl|__stdcall|__thiscall|__fastcall|Detour|ReplaceFunction|HookPacketHandler)')


@dataclass
class Row:
    addr: int
    kind: str
    refs: list = field(default_factory=list)
    symbol: str = ''
    location: str = ''
    status: str = 'unported'


def walk_sources(root):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in filenames:
            if name.endswith(SOURCE_EXT):
                yield os.path.join(dirpath, name)


def index_offsets(root):
    """addr -> (symbol, path, line) for every `// OFFSET:` tag; line is the declaration line (1-based)."""
    index = {}
    for path in walk_sources(root):
        with open(path, encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
        for i, line in enumerate(lines):
            m = OFFSET_RE.search(line)
            if not m:
                continue
            for j in range(i + 1, min(i + 6, len(lines))):
                text = lines[j].strip()
                if not text or text.startswith('//'):
                    continue
                s = SYMBOL_RE.search(text)
                if s:
                    index[int(m.group(1), 16)] = (s.group(1), path, j + 1)
                break
    return index


def scan_recon(root):
    """addr -> ['rel/path:line', ...] for client-range address literals in the reconstruction's sources."""
    refs = {}
    hints = {}
    for path in walk_sources(root):
        rel = os.path.relpath(path, root).replace(os.sep, '/')
        with open(path, encoding='utf-8', errors='ignore') as f:
            for n, line in enumerate(f, 1):
                for m in ADDR_RE.finditer(line):
                    addr = int(m.group(1), 16)
                    if CLIENT_LO <= addr <= CLIENT_HI:
                        refs.setdefault(addr, []).append(f'{rel}:{n}')
                        if FUNC_HINT_RE.search(line):
                            hints[addr] = 'func'
    scan_recon.hints = hints
    return refs


scan_recon.hints = {}


def build(decomp_root, recon_root):
    index = index_offsets(decomp_root)
    refs = scan_recon(recon_root)
    rows = {}
    for addr, where in refs.items():
        if addr in index:
            symbol, path, line = index[addr]
            location = f'{os.path.relpath(path, decomp_root).replace(os.sep, "/")}:{line}'
            rows[addr] = Row(addr, 'func', where, symbol, location, 'mapped')
        else:
            rows[addr] = Row(addr, scan_recon.hints.get(addr, 'unknown'), where)
    return rows


def merge(auto, manual):
    merged = dict(auto)
    for addr, row in manual.items():
        refs = auto[addr].refs if addr in auto else row.refs
        merged[addr] = Row(addr, row.kind, refs, row.symbol, row.location, row.status)
    return merged


def read_tsv(path):
    rows = {}
    if not os.path.exists(path):
        return rows
    with open(path, encoding='utf-8') as f:
        for line in f:
            if not line.strip() or line.startswith('addr\t'):
                continue
            cols = line.rstrip('\n').split('\t')
            cols += [''] * (len(COLUMNS) - len(cols))
            addr = int(cols[0], 16)
            refs = [r for r in cols[2].split(',') if r]
            rows[addr] = Row(addr, cols[1], refs, cols[3], cols[4], cols[5] or 'manual')
    return rows


def write_tsv(path, rows):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\t'.join(COLUMNS) + '\n')
        for addr in sorted(rows):
            r = rows[addr]
            f.write(f'0x{addr:08X}\t{r.kind}\t{",".join(r.refs)}\t{r.symbol}\t{r.location}\t{r.status}\n')


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--decomp', default='.')
    p.add_argument('--recon', required=True)
    p.add_argument('--out', default='CoA/addrmap.tsv')
    p.add_argument('--manual', default='CoA/addrmap.manual.tsv')
    args = p.parse_args(argv)
    rows = merge(build(args.decomp, args.recon), read_tsv(args.manual))
    write_tsv(args.out, rows)
    counts = {}
    for r in rows.values():
        counts[(r.kind, r.status)] = counts.get((r.kind, r.status), 0) + 1
    print(f'{len(rows)} addresses')
    for (kind, status), n in sorted(counts.items()):
        print(f'  {kind:8} {status:9} {n}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
