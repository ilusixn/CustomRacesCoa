"""Kyrian / Venthyr racial spells (Aetheria's 27 owned Spell.dbc rows, ids 111100-111113 / 111200-111212) into our client Spell.dbc, as
a new archive patch-ZZZZC.MPQ.  Visual ids / icon ids the client lacks are cleared / replaced.  The bat transform (misc value 9501110 in
the package) points at the vanilla Bat creature 20725 on our server (same remap in the server SQL)."""
import json, os, struct, sys
sys.path[:0] = [r'C:\CoA-Build\esteria_racials', r'C:\CoA-Build\tools', r'C:\CoA-Build\eunoia']
import estlib
B = chr(92)
cl = estlib.Client(estlib.OUR_DATA)
def eff(n):
    d, p = cl.read(estlib.dbc_name(n)); print(n, 'from', os.path.basename(p)); return estlib.DBC(d)
spell, icon, vis = eff('Spell.dbc'), eff('SpellIcon.dbc'), eff('SpellVisual.dbc')
J = json.load(open(r'C:\CoA-Build\sources\shotboom\aetheria-client\data\spells.json'))
assert J['fields'] == spell.fc, (J['fields'], spell.fc)
have_icon = {r[0] for r in icon.rows}
have_vis = {r[0] for r in vis.rows}

OLD = 'Requires Expert Riding and a normally flyable outdoor area. Cold Weather Flying is required in Northrend. Only usable outside combat.'
NEW = 'Usable at any time outside combat.'
for r in J['rows']:
    r['strings'] = {k: v.replace(OLD, NEW) for k, v in r['strings'].items()}
    words = list(r['words'])
    words[0] = r['id']
    for f, text in r['strings'].items():
        words[int(f)] = spell.add(text) if text else 0
    if words[110] == 9501110:
        words[110] = 20725
    # visuals (131, 132) and icon (133) must exist in our tables
    for f in (131, 132):
        if words[f] and words[f] not in have_vis:
            print('  spell', r['id'], 'visual', words[f], 'missing -> 0'); words[f] = 0
    if words[133] and words[133] not in have_icon:
        print('  spell', r['id'], 'icon', words[133], 'missing -> 1'); words[133] = 1
    spell.rows = [x for x in spell.rows if x[0] != r['id']]
    spell.rows.append(words)
spell.rows.sort(key=lambda x: x[0])
out = r'C:\CoA-Build\kv\out\racials_patch'
os.makedirs(out + B + 'DBFilesClient', exist_ok=True)
open(out + B + 'DBFilesClient' + B + 'Spell.dbc', 'wb').write(spell.bytes())
print('Spell.dbc rows', len(spell.rows))
