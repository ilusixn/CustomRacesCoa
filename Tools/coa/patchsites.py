"""Show each row of a reconstruction byte-patch table against the original exe: the containing decomp function,
the original and the patched bytes, both disassembled.

Usage:
    python Tools/coa/patchsites.py --exe D:/WoW/Coa/Ascension.exe \
        --table F:/WorkDir/ascension-extensions-reconstruction/src/Ascension/AscAttachPatches.generated.inc [--out file]

Rows: `{0xADDR, size, {0xNN, ...}},` as the generated tables write them.
"""
import argparse
import bisect
import os
import re
import sys

import capstone
import pefile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import addrmap  # noqa: E402

ROW_RE = re.compile(r'\{\s*(0x[0-9A-Fa-f]+)\s*,\s*(\d+)\s*,\s*\{([^}]*)\}')


def rows(table):
    for m in ROW_RE.finditer(open(table, encoding='utf-8').read()):
        data = bytes(int(b, 16) for b in re.findall(r'0x[0-9A-Fa-f]+', m.group(3)))
        yield int(m.group(1), 16), int(m.group(2)), data


def disasm(md, data, addr):
    return '; '.join(f'{i.mnemonic} {i.op_str}'.strip() for i in md.disasm(data, addr)) or data.hex()


def main(argv):
    p = argparse.ArgumentParser()
    p.add_argument('--exe', required=True)
    p.add_argument('--table', required=True)
    p.add_argument('--decomp', default='.')
    p.add_argument('--out')
    a = p.parse_args(argv)
    pe = pefile.PE(a.exe)
    base = pe.OPTIONAL_HEADER.ImageBase
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    idx = addrmap._scan_tags(a.decomp)
    funcs = sorted((k, v) for k, v in idx.items() if v[3] == 'func')
    keys = [k for k, _ in funcs]
    lines = []

    for addr, size, new in rows(a.table):
        i = bisect.bisect_right(keys, addr) - 1
        f, v = funcs[i]
        old = pe.get_data(addr - base, size)
        lines.append(f'0x{addr:X} [{size}] {v[0]}+0x{addr - f:X} ({v[1]}:{v[2]})')
        lines.append(f'    old: {old.hex()}  {disasm(md, old, addr)}')
        lines.append(f'    new: {new.hex()}  {disasm(md, new, addr)}')
        # Decode from the function start so the instructions around the patch are aligned.
        code = pe.get_data(f - base, addr - f + size + 32)

        for ins in md.disasm(code, f):
            if addr - 24 <= ins.address <= addr + size + 24:
                mark = '>' if ins.address < addr + size and ins.address + ins.size > addr else ' '
                lines.append(f'   {mark}{ins.address:08X} {ins.mnemonic} {ins.op_str}')

    text = '\n'.join(lines) + '\n'

    if a.out:
        open(a.out, 'w', encoding='utf-8').write(text)
    else:
        sys.stdout.write(text)


if __name__ == '__main__':
    main(sys.argv[1:])
