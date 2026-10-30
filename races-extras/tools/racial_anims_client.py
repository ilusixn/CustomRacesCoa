"""Racial spells that had no spell visual (cast animation / effect) get the matching stock one; Savage Bite (110920, Gnoll) plays Rip's (3941). Rewrites Spell.dbc into patch-ZZZZD.MPQ (keeps the Two Forms row)."""
import os, sys
sys.path[:0] = [r'C:\CoA-Build\esteria_racials', r'C:\CoA-Build\tools', r'C:\CoA-Build\eunoia']
import estlib
B = chr(92)
cl = estlib.Client(estlib.OUR_DATA)
d, p = cl.read(estlib.dbc_name('Spell.dbc')); print('from', os.path.basename(p))
spell = estlib.DBC(d)
byid = {r[0]: r for r in spell.rows}
assert 110690 in byid, 'the Two Forms row must be in the base'
CHANGES = {
    110920: 3941,                 # Gnoll Savage Bite: Rip
    14000512: 51, 14001032: 51,   # Cannibalize, Trail Snack: eating (Food)
    110707: 51,                   # Ingest Minerals
    111984: 7625,                 # Echo of the Naaru: Gift of the Naaru
    110400: 867, 111110: 867,     # Draconic Charge, Ascendant Charge: Charge
    110563: 14331,                # Embrace of the Loa: Berserking
    110040: 47, 110041: 47,       # Crimson Thirst: Blood Fury
}
for sid, visual in CHANGES.items():
    row = byid[sid]
    print(sid, 'visual', row[131], '->', visual)
    row[131] = visual
# Gnoll: Bone Gnaw (14000832, a Cannibalize copy) becomes Scavenger, with the eating visual
row = byid[14000832]
row[131] = 51
row[136] = spell.add('Scavenger')
row[170] = spell.add('A gnoll eats what it kills.\n\nWhen activated, regenerates $20578s1% of total health every $20578t1 sec for $20578d.  Only works on Humanoid or Undead corpses within $a1 yds.  Any movement, action, or damage taken while Scavenging will cancel the effect.')
print('14000832 ->', spell.s(row[136]), 'visual', row[131])
byid[110923][136] = spell.add('Skinner')   # the old passive Scavenger (+5 Skinning)
print('110923 ->', spell.s(byid[110923][136]))
out = r'C:\CoA-Build\kv\out\twoforms_patch'
os.makedirs(out + B + 'DBFilesClient', exist_ok=True)
open(out + B + 'DBFilesClient' + B + 'Spell.dbc', 'wb').write(spell.bytes())
print('rows', len(spell.rows))
