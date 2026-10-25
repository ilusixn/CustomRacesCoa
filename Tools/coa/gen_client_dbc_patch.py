"""Generate CoA/Source/Modules/ClientDbcPatch.generated.inc from the reconstruction's client-DBC patch table.

Usage:
    python Tools/coa/gen_client_dbc_patch.py --recon F:/WorkDir/ascension-extensions-reconstruction

The reconstruction's AscClientDbcPatch.generated.inc (plus the specs its AscClientDbcPatch.cpp writes by hand) gives,
per opcode, the client table (by its 32-bit container address), the 32-bit row size, the wire program and, per 32-bit
row dword, which wire value lands there. The x64 rows differ (pointers are 8 bytes), so each 32-bit dword is resolved to
the x64 record field it belongs to through the record header's `/* XXXX */` 32-bit offset comments, and a typed write
function is emitted per opcode: string values go into pointer fields, everything else is copied as a dword.
"""
import argparse
import os
import re
import sys

SPEC_RE = re.compile(r'^\{(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*"([^"]*)",\s*"((?:[^"\\]|\\.)*)"\}')
CLIENT_DB_RE = re.compile(r'COA_CLIENT_DB\((\w+)\)\s*//\s*(0x[0-9A-Fa-f]+)')
EXTERN_RE = re.compile(r'extern\s+(WowClientDB(?:_RLE)?)<(\w+)>\s+(\w+);')
STATIC_DB_RE = re.compile(r'//\s*STATIC:\s*(0x[0-9A-Fa-f]+)\s*\n\s*WowClientDB(?:_RLE)?<\w+>\s+(\w+);')
FIELD_RE = re.compile(r'/\*\s*([0-9A-Fa-f]{4})\s*\*/\s*([^;]+?)\s+(\w+)\s*(?:\[\s*(\w+)\s*\])?\s*;')

# AscClientDbcPatch.cpp's specs written by hand (the emulator could not run their handlers), copied verbatim.
HAND_SPECS = r'''
{0x932, 0xAD3D4C, 0x20, "dddddddd", "\001\002\003\004\005\006\007\010"},
{0x935, 0xAD48B0, 0x58, "ddddddddddddddL____ddddddd", "\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023\024\025\026"},
{0x937, 0xAD3080, 0x3C, "dddddddddLddddd", "\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017"},
{0x938, 0xAD30A4, 0x10, "ddL____d", "\001\002\003\004"},
{0x967, 0xAD3ED8, 0x34, "d________xxqzz", "\001\014\015\002\003\004\005\006\007\010\011\012\013"},
'''

# Opcodes whose handler ClientDbcPatchClient.cpp registers itself (it applies the writer with follow-ups of its own).
HAND_HANDLERS = {0x932, 0x934}

# Opcodes whose 32-bit handler stores a wire dword into a pointer slot (0x960 TOTEM_CATEGORY: the low half of its
# u64 lands in m_name). The x64 row gets the dword zero-extended, as meaningless a pointer as the original's.
DWORD_INTO_POINTER = {0x960}

# 0x92A SPELL is written by hand in ClientDbcPatchClient.cpp from this identity map over the 0x2A8-byte image (the four
# sized strings land at +0x220, +0x228, +0x224, +0x22C).
SPELL_SPEC = (0x92A, 0xAD49D0, 0x2A8)


def c_unescape(s):
    out = []
    i = 0
    while i < len(s):
        if s[i] == '\\':
            m = re.match(r'[0-7]{1,3}', s[i + 1:])
            if m:
                out.append(int(m.group(0), 8))
                i += 1 + len(m.group(0))
                continue
            raise ValueError('unsupported escape in ' + s)
        out.append(ord(s[i]))
        i += 1
    return out


def value_kinds(program):
    """Per wire value: 's' for a string (a pointer the handler allocates), 'd' for a dword."""
    kinds = []
    packing_b = 0
    packing_p = False
    for c in program:
        if c in 'bhd':
            kinds.append('d')
        elif c == 'q':
            kinds += ['d', 'd']
        elif c == 'x':
            kinds += ['d'] * 4
        elif c in 'szL':
            kinds.append('s')
        elif c == '_':
            pass
        elif c == 'p':
            if not packing_p:
                kinds.append('d')
                packing_p = True
        elif c == '|':
            packing_p = False
        elif c == 'B':
            if packing_b == 0:
                kinds.append('d')
            packing_b = (packing_b + 1) & 3
        else:
            raise ValueError('unknown program code %r in %s' % (c, program))
    return kinds


def parse_record(path, row_size):
    fields = []
    with open(path, encoding='utf-8') as f:
        for line in f:
            m = FIELD_RE.search(line)
            if m:
                fields.append((int(m.group(1), 16), m.group(2).strip(), m.group(3), m.group(4)))
    out = []
    for i, (off, typ, name, count) in enumerate(fields):
        end = fields[i + 1][0] if i + 1 < len(fields) else row_size
        n = 1
        if count:
            n = int(count, 0) if re.match(r'^(0x)?[0-9A-Fa-f]+$', count) else None
        out.append({'off': off, 'size': end - off, 'type': typ, 'name': name, 'count': n, 'array': count is not None})
    return out


def field_at(fields, byte):
    for f in fields:
        if f['off'] <= byte < f['off'] + f['size']:
            return f
    return None


def emit_writer(op, rec, fields, row_size, mapping, kinds, problems):
    lines = ['    void Write%03X(%s* row, const uint64_t* v) {' % (op, rec)]
    for d in range(row_size // 4):
        k = mapping[d] if d < len(mapping) else 0
        if not k:
            continue
        byte = d * 4
        f = field_at(fields, byte)
        if f is None:
            problems.append('0x%03X: %s +0x%X has no field' % (op, rec, byte))
            continue
        is_ptr = '*' in f['type']
        kind = kinds[k - 1] if k - 1 < len(kinds) else '?'
        if kind == '?':
            problems.append('0x%03X: value %d past the program' % (op, k))
            continue
        if is_ptr:
            elem = f['size'] // (f['count'] or 1)
            if elem != 4 or (byte - f['off']) % 4:
                problems.append('0x%03X: %s.%s pointer element size %d' % (op, rec, f['name'], elem))
                continue
            if kind != 's' and op not in DWORD_INTO_POINTER:
                problems.append('0x%03X: %s.%s is a pointer but value %d is a dword' % (op, rec, f['name'], k))
            idx = (byte - f['off']) // 4
            target = 'row->%s[%d]' % (f['name'], idx) if f['array'] else 'row->%s' % f['name']
            lines.append('        PutPtr(%s, v[%d]);' % (target, k - 1))
        else:
            if kind == 's':
                problems.append('0x%03X: %s.%s is not a pointer but value %d is a string' % (op, rec, f['name'], k))
            lines.append('        Put(row->%s, %d, v[%d]);' % (f['name'], byte - f['off'], k - 1))
    lines.append('    }')
    return lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--recon', required=True)
    ap.add_argument('--root', default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    args = ap.parse_args()
    root = args.root

    specs = []
    gen = os.path.join(args.recon, 'src', 'Ascension', 'AscClientDbcPatch.generated.inc')
    with open(gen, encoding='utf-8') as f:
        text = f.read()
    for line in (text + HAND_SPECS).splitlines():
        m = SPEC_RE.match(line.strip())
        if m:
            comment = line.split('//', 1)[1].strip() if '//' in line else 'by hand'
            specs.append((int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16), m.group(4), c_unescape(m.group(5)), comment))

    containers = {}
    with open(os.path.join(root, 'CoA', 'Source', 'Modules', 'ClientDbcs.generated.inc'), encoding='utf-8') as f:
        for m in CLIENT_DB_RE.finditer(f.read()):
            containers[int(m.group(2), 16)] = m.group(1)
    with open(os.path.join(root, 'WoW', 'Source', 'DB', 'DBClient', 'DBClient.cpp'), encoding='utf-8') as f:
        for m in STATIC_DB_RE.finditer(f.read()):
            containers.setdefault(int(m.group(1), 16), m.group(2))
    tables = {}
    with open(os.path.join(root, 'WoW', 'Source', 'DB', 'DBClient', 'DBClient.h'), encoding='utf-8') as f:
        for m in EXTERN_RE.finditer(f.read()):
            tables[m.group(3)] = (m.group(1), m.group(2))

    problems = []
    body = []
    regs = []
    includes = set()
    for op, cont, size, program, mapping, comment in sorted(specs, key=lambda s: s[0]):
        db = containers.get(cont)
        if not db or db not in tables:
            problems.append('0x%03X: container 0x%X has no client table' % (op, cont))
            continue
        kind, rec = tables[db]
        header = os.path.join(root, 'WoW', 'Source', 'DB', 'Rec', rec + '.h')
        fields = parse_record(header, size)
        includes.add('WoW/Source/DB/Rec/%s.h' % rec)
        body.append('    // 0x%03X %s -> %s (%s, 32-bit row 0x%X): "%s"' % (op, comment, db, rec, size, program))
        body += emit_writer(op, rec, fields, size, mapping, value_kinds(program), problems)
        body.append('')
        regs.append('    COA_CLIENT_DBC_PATCH(0x%03X, %s, %s, "%s", Write%03X)' % (op, db, rec, program, op))
        if op not in HAND_HANDLERS:
            regs.append('    COA_PACKET(0x%03X, OnPatch);' % op)

    op, cont, size = SPELL_SPEC
    db = containers[cont]
    kind, rec = tables[db]
    fields = parse_record(os.path.join(root, 'WoW', 'Source', 'DB', 'Rec', rec + '.h'), size)
    includes.add('WoW/Source/DB/Rec/%s.h' % rec)
    strings = {0x220: 0, 0x228: 1, 0x224: 2, 0x22C: 3}
    lines = ['    // 0x92A SPELL (FUN_10231de0): the 0x2A8-byte 32-bit image (dwords) into the x64 row; the four sized strings',
             '    // (s[0..3]) at +0x220, +0x228, +0x224, +0x22C.',
             '    void WriteSpellImage(%s* row, const uint32_t* image, const uint64_t* s) {' % rec]
    for d in range(size // 4):
        byte = d * 4
        f = field_at(fields, byte)
        if f is None:
            problems.append('SPELL +0x%X has no field' % byte)
            continue
        if '*' in f['type']:
            if byte not in strings:
                problems.append('SPELL pointer %s not a string slot' % f['name'])
                continue
            target = 'row->%s[%d]' % (f['name'], (byte - f['off']) // 4) if f['array'] else 'row->%s' % f['name']
            lines.append('        PutPtr(%s, s[%d]);' % (target, strings[byte]))
        else:
            lines.append('        Put(row->%s, %d, image[%d]);' % (f['name'], byte - f['off'], d))
    lines.append('    }')
    body += lines

    if problems:
        for p in problems:
            print('error:', p, file=sys.stderr)
        sys.exit(1)

    out = ['// AUTO-GENERATED by Tools/coa/gen_client_dbc_patch.py from the reconstruction\'s AscClientDbcPatch.generated.inc',
           '// (and AscClientDbcPatch.cpp\'s hand-written specs). Do not edit by hand.',
           '#ifdef COA_CLIENT_DBC_PATCH_INCLUDES']
    out += ['#include "%s"' % i for i in sorted(includes)]
    out += ['#endif', '', '#ifdef COA_CLIENT_DBC_PATCH_WRITERS']
    out += body
    out += ['#endif', '', '#ifdef COA_CLIENT_DBC_PATCH_REGISTRATIONS']
    out += regs
    out += ['#endif', '']
    path = os.path.join(root, 'CoA', 'Source', 'Modules', 'ClientDbcPatch.generated.inc')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(out))
    print('%d patches written to %s' % (sum('COA_CLIENT_DBC_PATCH' in r for r in regs), path))


if __name__ == '__main__':
    main()
