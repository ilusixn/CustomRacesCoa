"""Disassemble the same address range in two 32-bit client executables side by side.

Usage: python Tools/coa/exediff.py <exeA> <exeB> <addr> [length]
Used to judge Ascension.exe patch sites against a reference 3.3.5a exe (see CoA/DIFFERENCES.md).
Needs the `capstone` package.
"""
import struct
import sys

import capstone

IMAGE_BASE = 0x400000


def load_text(path):
    data = open(path, 'rb').read()
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    table = pe + 24 + struct.unpack_from('<H', data, pe + 20)[0]
    for i in range(count):
        name, _, va, size, raw = struct.unpack_from('<8sIIII', data, table + i * 40)
        if name.rstrip(b'\0') == b'.text':
            return va, data[raw:raw + size]
    raise ValueError(f'{path}: no .text section')


def disasm(path, addr, length):
    va, text = load_text(path)
    offset = addr - IMAGE_BASE - va
    cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    return [f'{i.address:08X}  {i.bytes.hex():<16} {i.mnemonic} {i.op_str}' for i in cs.disasm(text[offset:offset + length], addr)]


def main(argv):
    a, b, addr = argv[1], argv[2], int(argv[3], 16)
    length = int(argv[4], 0) if len(argv) > 4 else 0x40
    for path in (a, b):
        print(f'== {path}')
        print('\n'.join(disasm(path, addr, length)))


if __name__ == '__main__':
    main(sys.argv)
