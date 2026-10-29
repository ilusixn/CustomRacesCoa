"""Merge the Kyrian (package race 60 -> our 107) and Venthyr (61 -> 101) rows of the Kyrian-Venthyr package into our DBC files.
   python merge_dbc.py client|server  -> writes C:/CoA-Build/kv/out/<side>/DBFilesClient/*.dbc (client) or .../dbc/*.dbc (server)
"""
import base64, json, os, struct, sys
sys.path[:0] = [r'C:\CoA-Build\kv', r'C:\CoA-Build\esteria_racials']
import effread, estlib

BS = chr(92)
PKG = r'C:\Users\ilusi\Downloads\Kyrian-Venthyr-Approved-20261021\Kyrian-Venthyr-Approved-20261021'
RACE = {60: 107, 61: 101}          # package race id -> our race id
TWIN = {107: 11, 101: 5}           # same race-mask bit: start data, classes and racials come from the twin
SIDE = sys.argv[1]
OUT = r'C:\CoA-Build\kv\out' + BS + SIDE
SERVER_DIR = 'C:/CoA-Build/kv/backup/dbc'      # the original server DBCs (the live ones already hold the races)
rows_json = json.load(open(PKG + BS + SIDE + BS + 'dbc-rows.json'))


def load(table):
    if SIDE == 'client':
        d, p = effread.read('DBFilesClient' + BS + table + '.dbc')
        assert d, table
        return d, p
    p = SERVER_DIR + BS + table + '.dbc'
    return open(p, 'rb').read(), p


def package_rows(table, T):
    """The package rows as lists of ints with their string fields added to T's string block."""
    spec = rows_json[table]
    out = []
    for r in spec['rows']:
        raw = base64.b64decode(r['bytes'])
        vals = list(struct.unpack('<%dI' % (len(raw) // 4), raw))
        assert len(vals) == len(T.rows[0]), (table, len(vals), len(T.rows[0]))
        for k, text in r['strings'].items():
            vals[int(k)] = T.add(text)
        out.append(vals)
    return out


def next_id(T):
    return max(r[0] for r in T.rows) + 1


written = {}


def save(table, data):
    os.makedirs(OUT, exist_ok=True)
    p = OUT + BS + table + '.dbc'
    open(p, 'wb').write(data)
    written[table] = len(data)


# --- display / model ids: keep the package ids when free, else renumber (the references are rewritten)
cmd_d, _ = load('CreatureModelData')
cdi_d, _ = load('CreatureDisplayInfo')
CMD = estlib.DBC(cmd_d)
CDI = estlib.DBC(cdi_d)
model_map, display_map = {}, {}
used_models = {r[0] for r in CMD.rows}
used_displays = {r[0] for r in CDI.rows}
new_cmd = package_rows('CreatureModelData', CMD)
new_cdi = package_rows('CreatureDisplayInfo', CDI)
nm, nd = next_id(CMD), next_id(CDI)
for r in new_cmd:
    old = r[0]
    if old in used_models:
        r[0] = nm
        nm += 1
    model_map[old] = r[0]
for r in new_cdi:
    old = r[0]
    if old in used_displays:
        r[0] = nd
        nd += 1
    display_map[old] = r[0]
    r[1] = model_map[r[1]]
for r in new_cdi:
    if r[0] in (60083, 60084):      # Kyrian: a little smaller than the package so the creation / selection camera frames the wings
        r[4] = struct.unpack('<I', struct.pack("<f", 0.70))[0]
CMD.rows += new_cmd
CDI.rows += new_cdi
save('CreatureModelData', CMD.bytes())
save('CreatureDisplayInfo', CDI.bytes())
print('models', model_map, 'displays', display_map)

# --- ChrRaces
d, p = load('ChrRaces')
T = estlib.DBC(d)
twin_rows = {r[0]: r for r in T.rows}
new = package_rows('ChrRaces', T)
for r in new:
    pkg = r[0]
    r[0] = RACE[pkg]
    r[4] = display_map[r[4]]
    r[5] = display_map[r[5]]
    twin = twin_rows[TWIN[r[0]]]
    r[12] = twin[12]            # cinematic of the twin's start (Draenei / Undead)
assert not ({r[0] for r in new} & {r[0] for r in T.rows}), 'race id already used'
T.rows += new
save('ChrRaces', T.bytes())

# --- CharSections / CharHairGeosets: new row ids, race remapped
for table in ('CharSections', 'CharHairGeosets'):
    d, p = load(table)
    T = estlib.DBC(d)
    nid = next_id(T)
    new = package_rows(table, T)
    for r in new:
        r[0] = nid
        nid += 1
        r[1] = RACE[r[1]]
    T.rows += new
    save(table, T.bytes())

# --- CharacterFacialHairStyles: race first, no id
d, p = load('CharacterFacialHairStyles')
T = estlib.DBC(d)
new = package_rows('CharacterFacialHairStyles', T)
for r in new:
    r[0] = RACE[r[0]]
T.rows += new
save('CharacterFacialHairStyles', T.bytes())

# --- BarberShopStyle (race at field 37) and NameGen (race at field 2)
for table, field in (('BarberShopStyle', 37), ('NameGen', 2)):
    d, p = load(table)
    T = estlib.DBC(d)
    nid = next_id(T)
    new = package_rows(table, T)
    for r in new:
        r[0] = nid
        nid += 1
        r[field] = RACE[r[field]]
    T.rows += new
    save(table, T.bytes())

# --- CharBaseInfo: (race, class) byte pairs; every class the twin has
d, p = load('CharBaseInfo')
n, fc, rs, ss = struct.unpack_from('<4I', d, 4)
body = d[20:20 + n * rs]
pairs = [(body[i * 2], body[i * 2 + 1]) for i in range(n)]
extra = []
for race, twin in TWIN.items():
    extra += [(race, c) for (r, c) in pairs if r == twin]
assert not any(r in (101, 107) for r, _ in pairs), 'CharBaseInfo already has the races'
pairs += extra
body = b''.join(bytes(pr) for pr in pairs)
save('CharBaseInfo', struct.pack('<4s4I', b'WDBC', len(pairs), fc, rs, ss) + body + d[20 + n * rs:])
print(SIDE, 'written', written)
