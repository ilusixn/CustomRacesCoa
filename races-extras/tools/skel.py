import sys, io
sys.path[:0] = [r'C:\CoA-Build\kv', r'C:\CoA-Build\logo', r'C:\CoA-Build\esteria_racials']
import effread, estlib, blp
from PIL import Image
BS = chr(92)
d, p = effread.read('DBFilesClient' + BS + 'ChrRaces.dbc')
R = estlib.DBC(d)
for r in R.rows:
    if r[0] in (26, 37):
        print('race', r[0], R.s(r[14]), 'display', r[4], r[5], 'flags', hex(r[1]), 'file', R.s(r[11]))
d, p = effread.read('DBFilesClient' + BS + 'CharSections.dbc')
T = estlib.DBC(d)
seen = []
for r in T.rows:
    if r[1] in (26, 37) and r[3] in (0, 1) and r[2] == 0:
        seen.append((r[1], r[3], r[8], r[9], T.s(r[4]), T.s(r[5])))
print(len(seen))
for x in seen[:12]:
    print(x)
d, p = effread.read('DBFilesClient' + BS + 'CreatureDisplayInfo.dbc')
C = estlib.DBC(d)
d, p2 = effread.read('DBFilesClient' + BS + 'CreatureModelData.dbc')
M = estlib.DBC(d)
mm = {r[0]: r for r in M.rows}
for r in R.rows:
    if r[0] in (26, 37):
        for disp in (r[4], r[5]):
            row = [c for c in C.rows if c[0] == disp]
            if row:
                m = mm.get(row[0][1])
                print('display', disp, 'model', row[0][1], M.s(m[2]) if m else None, 'tex', [C.s(x) for x in row[0][6:9]])
