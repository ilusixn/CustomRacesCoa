import json, base64, struct, sys
sys.path[:0] = [r'C:\CoA-Build\esteria_racials']
import estlib
BS = chr(92)
C = estlib.Client(estlib.OUR_DATA)
base = r'C:\Users\ilusi\Downloads\Kyrian-Venthyr-Approved-20261021\Kyrian-Venthyr-Approved-20261021'
for side in ('client', 'server'):
    d = json.load(open(base + BS + side + BS + 'dbc-rows.json'))
    print('==', side, list(d))
    for t, v in d.items():
        rows = v['rows']
        info = ''
        if side == 'client':
            raw, p = C.read('DBFilesClient' + BS + t + '.dbc')
            if raw:
                T = estlib.DBC(raw)
                info = 'ours: %d rows, %d fields, from %s' % (len(T.rows), len(T.rows[0]) if T.rows else -1, p.split(BS)[-1])
            else:
                info = 'ours: MISSING'
        print(t, 'fields', v['fields'], 'stride', v['stride'], 'rows', len(rows), info)
        if rows:
            raw = base64.b64decode(rows[0]['bytes'])
            n = len(raw) // 4
            print('   first row', struct.unpack('<%dI' % n, raw[:n * 4])[:14] if n else raw.hex(), 'strings', dict(list(rows[0]['strings'].items())[:2]))
