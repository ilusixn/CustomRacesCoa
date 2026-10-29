"""Creation-screen racial text of Kyrian (107) and Venthyr (101): Aetheria's own racials (names, icons, descriptions from spells.json).
The old generic actives (Steward's Resolve / Anima Frenzy, inserted after the table) are dropped."""
import json
BS = chr(92)
LUA = r'C:\CoA-Build\slime\out\faun_fix_patch\Interface\GlueXML\CharacterCreate.lua'
J = {r['id']: r for r in json.load(open(r'C:\CoA-Build\sources\shotboom\aetheria-client\data\spells.json'))['rows']}
ICON = {111104: 'Spell_Magic_FeatherFall', 111105: 'Spell_Holy_Purify', 111106: 'Spell_Holy_BlessingOfStrength', 111107: 'Ability_Rogue_Sprint',
        111108: 'INV_Misc_Gem_01', 111110: 'Ability_Warrior_Charge', 111200: 'Spell_Shadow_LifeDrain02', 111201: 'Spell_Nature_Sleep',
        111202: 'Ability_Racial_Cannibalize', 111203: 'Ability_Racial_Cannibalize', 111204: 'Ability_Racial_Cannibalize',
        111206: 'Ability_Racial_Cannibalize', 111209: 'Ability_Racial_Cannibalize'}


def lua(text):
    text = text.replace('Requires Expert Riding and a normally flyable outdoor area. Cold Weather Flying is required in Northrend. Only usable outside combat.', 'Usable at any time outside combat.')
    return text.replace(BS, BS + BS).replace('"', BS + '"').replace('\n', ' ')


def entry(i):
    r = J[i]
    kind = 'passive' if r['words'][4] & 0x40 else 'active'
    return '{"%s", "%s", "%s", "%s"}' % (lua(r['strings']['136']), ICON[i], kind, lua(r['strings']['170']))


lists = {107: [111104, 111105, 111106, 111107, 111108, 111110], 101: [111200, 111201, 111202, 111203, 111204, 111206, 111209]}
lines = open(LUA, encoding='utf-8', newline='').read().split('\n')
out, changed = [], 0
for l in lines:
    for race, ids in lists.items():
        if l.startswith('\tCHAR_CREATE_COA_RACIALS[%d] = ' % race):
            l = '\tCHAR_CREATE_COA_RACIALS[%d] = {%s}' % (race, ', '.join(entry(i) for i in ids)); changed += 1
        elif l.startswith('\tif CHAR_CREATE_COA_RACIALS[%d] then table.insert(CHAR_CREATE_COA_RACIALS[%d]' % (race, race)):
            l = ''; changed += 1
    out.append(l)
open(LUA, 'w', encoding='utf-8', newline='').write('\n'.join(out))
print('changed', changed)
