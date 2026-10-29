"""Port the retail-related INSERTIONS of an STR (Esteria) file into the CoA64 copy.
usage: port_hunks.py <relative path> [regex] [--dry]
Aligns the two files line by line (LF-normalised); every block that STR has and CoA64 lacks (pure insertion) whose lines match the
regex (default: Retail) is inserted into the CoA64 file at the aligned spot; blocks that REPLACE CoA64 lines are only listed (MANUAL),
never applied, so CoA-specific code is never overwritten.  Line endings of the CoA64 file are kept."""
import difflib, re, sys, os
STR = r'C:\CoA-Build\sources\esteria\wowdecomp'
COA = r'C:\CoA-Build\coa64'
rel = sys.argv[1]
rx = re.compile(sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith('--') else r'Retail|RETAIL|retail')
dry = '--dry' in sys.argv
a_raw = open(os.path.join(COA, rel), 'rb').read().decode('utf-8', 'replace')
crlf = a_raw.count('\r\n') > a_raw.count('\n') / 2
a = a_raw.replace('\r\n', '\n').split('\n')
b = open(os.path.join(STR, rel), 'rb').read().decode('utf-8', 'replace').replace('\r\n', '\n').split('\n')
ops = difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes()
out = list(a)
applied = manual = 0
for tag, i1, i2, j1, j2 in reversed(ops):
    if tag == 'equal':
        continue
    added = b[j1:j2]
    if not rx.search('\n'.join(added)):
        continue
    if tag == 'insert':
        out[i1:i1] = added
        applied += 1
    else:
        manual += 1
        print('MANUAL (%s) CoA lines %d-%d: removed %d added %d | %s' % (tag, i1 + 1, i2, i2 - i1, j2 - j1,
              ' // '.join(x.strip() for x in added if rx.search(x))[:200]))
        if '--show' in sys.argv:
            NLC = chr(10)
            print('  --- CoA64:'); print(NLC.join('  ' + l for l in a[max(0, i1 - 2):i2 + 2]))
            print('  +++ STR:'); print(NLC.join('  ' + l for l in added))
text = '\n'.join(out)
if crlf:
    text = text.replace('\n', '\r\n')
print(rel, 'inserted', applied, 'blocks;', manual, 'need manual work')
if not dry:
    open(os.path.join(COA, rel), 'wb').write(text.encode('utf-8'))
