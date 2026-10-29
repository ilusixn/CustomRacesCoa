"""Worgen form switch: a new spell 110690 'Two Forms' cloned from Visage (110685) (dummy, instant, no cooldown), in patch-ZZZZD.MPQ."""
import os, sys
sys.path[:0] = [r'C:\CoA-Build\esteria_racials', r'C:\CoA-Build\tools', r'C:\CoA-Build\eunoia']
import estlib
B = chr(92)
cl = estlib.Client(estlib.OUR_DATA)
d, p = cl.read(estlib.dbc_name('Spell.dbc')); print('from', os.path.basename(p))
spell = estlib.DBC(d)
src = next(r for r in spell.rows if r[0] == 110685)
words = list(src)
words[0] = 110690
words[133] = 5772                      # the Worgen icon Two Forms already used
for f, text in {136: 'Two Forms', 170: 'Switch between your human and worgen forms.'}.items():
    words[f] = spell.add(text)
for f in (137, 153, 154):              # rank / tooltip-related string columns: keep the clone's own
    pass
spell.rows = [x for x in spell.rows if x[0] != 110690]
spell.rows.append(words)
spell.rows.sort(key=lambda x: x[0])
out = r'C:\CoA-Build\kv\out\twoforms_patch'
os.makedirs(out + B + 'DBFilesClient', exist_ok=True)
open(out + B + 'DBFilesClient' + B + 'Spell.dbc', 'wb').write(spell.bytes())
print('rows', len(spell.rows), 'name col', words[136], 'desc col', words[170])
