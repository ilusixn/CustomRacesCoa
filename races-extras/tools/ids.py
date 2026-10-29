import sys
sys.path[:0] = [r'C:\CoA-Build\esteria_racials']
import estlib
BS = chr(92)
C = estlib.Client(estlib.OUR_DATA)
d, p = C.read('DBFilesClient' + BS + 'ChrRaces.dbc')
R = estlib.DBC(d)
print(p, len(R.rows), 'fields per row', len(R.rows[0]))
used = {}
for r in R.rows:
    used[r[0]] = (R.s(r[14]), 'H' if r[13] else 'A', hex(r[1]))
print(sorted(used.items()))
free = [i for i in range(33, 128) if i not in used]
print('free ids', free)
for i in free:
    twin = ((i - 1) & 31) + 1
    t = used.get(twin)
    print(i, 'twin', twin, t)
