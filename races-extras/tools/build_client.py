"""Builds patch-ZZZE.MPQ (Kyrian 107 / Venthyr 101 for our Ascension client) and the client-root files.
   Needs: python merge_dbc.py client (out/client/*.dbc)."""
import io, os, shutil, sys, ctypes
sys.path[:0] = [r'C:\CoA-Build\kv', r'C:\CoA-Build\logo', r'C:\CoA-Build\esteria_racials', r'C:\CoA-Build\eunoia', r'C:\CoA-Build\tools']
import effread, blp, mpqread, storm
from PIL import Image

BS = chr(92)
PKG = r'C:\Users\ilusi\Downloads\Kyrian-Venthyr-Approved-20261021\Kyrian-Venthyr-Approved-20261021'
STAGE = r'C:\CoA-Build\kv\out\patch'
ROOT = r'C:\CoA-Build\kv\out\clientroot'
ARCHIVE = r'C:\CoA-Build\kv\out\patch-ZZZE.MPQ'
shutil.rmtree(STAGE, ignore_errors=True)
shutil.rmtree(ROOT, ignore_errors=True)
os.makedirs(STAGE)
os.makedirs(ROOT)


def put(rel, data):
    p = os.path.join(STAGE, *rel.split(BS))
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, 'wb').write(data)


# 1. DBC
for name in os.listdir(r'C:\CoA-Build\kv\out\client'):
    if name.endswith('.dbc'):
        put('DBFilesClient' + BS + name, open(os.path.join(r'C:\CoA-Build\kv\out\client', name), 'rb').read())

# 2. models, skins, animations, textures (the equipment fitting atlases are not used by our client yet)
m = mpqread.MPQ(PKG + BS + 'client' + BS + 'race-assets.mpq')
count = 0
for name in m.listfile():
    if name.lower().startswith('custom' + BS + 'equipment') and not name.lower().endswith('empty-garment.blp'):
        continue
    low = name.lower()
    data = m.read(name)
    if low.endswith('vampire_male.m2') or low.endswith('vampire_female.m2') or low.endswith('kyrianmale.m2') or low.endswith('kyrianfemale.m2'):
        data = open(r'C:\CoA-Build\slime\out\emotes' + BS + low.split(BS)[-1], 'rb').read()
    if name.lower().endswith('kyrianmale00.skin') or name.lower().endswith('kyrianfemale00.skin'):
        import venthyr_naked
        data, total = venthyr_naked.trim_skin(data, venthyr_naked.KYRIAN_HIDE | venthyr_naked.LEGACY_BODY)
        print('kyrian skin trimmed', name, total, 'indices')
    if name.lower().endswith('vampire_male00.skin') or name.lower().endswith('vampire_female00.skin'):
        import venthyr_naked
        data = open(r'C:\CoA-Build\slime\out\venthyr_robe' + BS + name.split(BS)[-1], 'rb').read()   # robe re-skinned on the legs (venthyr_robe_skin.py)
        data, total = venthyr_naked.trim_skin(data, venthyr_naked.HIDE_MALE if 'male00' in name.lower() and 'female' not in name.lower() else venthyr_naked.HIDE_FEMALE)
        print('venthyr skin trimmed', name, total, 'indices')
    if name.lower().endswith('.blp') and data[:4] == b'BLP2' and data[8] == 3 and data[10] != 8:
        data = data[:10] + bytes([8]) + data[11:]      # uncompressed ARGB with alpha encoding 0 is read as DXT1 by the 32-bit client
    put(name, data)
    count += 1
put('Character' + BS + 'Vrykul' + BS + 'Male' + BS + 'VrykulMale.m2', open(r'C:\CoA-Build\slime\out\emotes\VrykulMale.m2', 'rb').read())
for key in ('Character\\Tuskarr\\Male\\TuskarrMale.m2', 'Creature\\WhimMurloc\\WhimPlayer.m2', 'Creature\\Murloc\\MurlocPlayer.m2', 'Character\\Furbolg\\FurbolgRace.m2'):
    put(key, open(r'C:\CoA-Build\slime\out\sheath' + BS + key, 'rb').read())
print('assets', count)

# 3. banks next to the DLL
bank = PKG + BS + 'client' + BS + 'Data' + BS + 'Retail' + BS
for f in ('EsteriaKyrian.bin', 'EsteriaKyrianTextures.bin', 'EsteriaVenthyr.bin', 'EsteriaVenthyrTextures.bin'):
    shutil.copy2(bank + f, ROOT)
shutil.copy2(r'C:' + BS + 'CoA-Build' + BS + 'thinhuman' + BS + 'new' + BS + 'EsteriaThinHuman.bin', ROOT)
shutil.copy2(r'C:' + BS + 'CoA-Build' + BS + 'thinhuman' + BS + 'new' + BS + 'EsteriaThinHumanTextures.bin', ROOT)
shutil.copytree(r'C:' + BS + 'CoA-Build' + BS + 'thinhuman' + BS + 'native', STAGE, dirs_exist_ok=True)
shutil.copy2(r'C:\CoA-Build\esteria\dll2\src\client-customization\out\EsteriaAppearance.dll', ROOT)
import venthyr_naked
open(os.path.join(ROOT, 'EsteriaKyrian.bin'), 'wb').write(venthyr_naked.filter_bank(open(bank + 'EsteriaKyrian.bin', 'rb').read(), venthyr_naked.KYRIAN_DROP))
open(os.path.join(ROOT, 'EsteriaVenthyr.bin'), 'wb').write(venthyr_naked.filter_bank(open(bank + 'EsteriaVenthyr.bin', 'rb').read()))

# 4. creation screen: race buttons, ids, options, lore, background
cc, _ = effread.read('Interface' + BS + 'GlueXML' + BS + 'CharacterCreate.lua')
gp, _ = effread.read('Interface' + BS + 'GlueXML' + BS + 'GlueParent.lua')
cc = cc.decode('latin-1')
gp = gp.decode('latin-1')


def once(text, old, new):
    assert text.count(old) == 1, (old[:50], text.count(old))
    return text.replace(old, new)


cc = once(cc, 'SABERONHORDE = 64}', 'SABERONHORDE = 64, KYRIANALLIANCE = 60, VENTHYRHORDE = 61}')
cc = once(cc, 'CHAR_CREATE_RACE_IDS["Saberon"] = {97}',
          'CHAR_CREATE_RACE_IDS["Saberon"] = {97}\nCHAR_CREATE_RACE_IDS["Kyrian"] = {107}\nCHAR_CREATE_RACE_IDS["Venthyr"] = {101}')
cc = once(cc, '\tCHAR_CREATE_EXTRA_RACE_ICONS["Saberon"] = "Interface\\\\Icons\\\\CoA_Race_Saberon"',
          '\tCHAR_CREATE_EXTRA_RACE_ICONS["Saberon"] = "Interface\\\\Icons\\\\CoA_Race_Saberon"\n'
          '\tCHAR_CREATE_EXTRA_RACE_ICONS["Kyrian"] = "Interface\\\\Icons\\\\CoA_Race_Kyrian"\n'
          '\tCHAR_CREATE_EXTRA_RACE_ICONS["Venthyr"] = "Interface\\\\Icons\\\\CoA_Race_Venthyr"')
LORE_K = ('Winged shepherds of the afterlife, the kyrian guide the souls of the worthy to their rest. Long bound to the '
          'discipline of the Bastion, a few of them now walk the living world, carrying their ancient duty of mercy and '
          'judgement with them.')
LORE_V = ('Proud nobles of the shadowed halls of Revendreth, the venthyr rule through lineage, pride and a hunger '
          'that never fades. Some have left their dark courts for the open world, where their old grudges and '
          'appetites follow them.')
cc = once(cc, '\t\t["Sethrak"] = [==[Hailing',
          '\t\t["Kyrian"] = [==[%s]==],\n\t\t["Venthyr"] = [==[%s]==],\n\t\t["Sethrak"] = [==[Hailing' % (LORE_K, LORE_V))
gp = once(gp, '["NAGA_"] = "ORC",', '["NAGA_"] = "ORC", ["KYRIANALLIANCE"] = "HUMAN", ["VENTHYRHORDE"] = "ORC",')
# Slime Giant (112 Alliance / 104 Horde, Esteria id 65)
cc = once(cc, 'VENTHYRHORDE = 61}', 'VENTHYRHORDE = 61, SLIMEGIANTHORDE = 65}')
cc = once(cc, 'CHAR_CREATE_RACE_IDS["Venthyr"] = {101}',
          'CHAR_CREATE_RACE_IDS["Venthyr"] = {101}\nCHAR_CREATE_RACE_IDS["Slime Giant"] = {104}\n'
          'if CHAR_CREATE_MALE_ONLY_RACES then CHAR_CREATE_MALE_ONLY_RACES["Slime Giant"] = true end')
cc = once(cc, '\tCHAR_CREATE_EXTRA_RACE_ICONS["Venthyr"] = "Interface\\\\Icons\\\\CoA_Race_Venthyr"',
          '\tCHAR_CREATE_EXTRA_RACE_ICONS["Venthyr"] = "Interface\\\\Icons\\\\CoA_Race_Venthyr"\n'
          '\tCHAR_CREATE_EXTRA_RACE_ICONS["Slime Giant"] = "Interface\\\\Icons\\\\CoA_Race_SlimeGiant"')
LORE_S = ('A towering mass of living slime wrapped around a restless skeleton, the slime giants are born where '
          'ooze and the dead meet. Slow to anger and hard to stop, they lumber out of the sewers and the swamps '
          'to find a place among the living.')
cc = once(cc, '\t\t["Kyrian"] = [==[', '\t\t["Slime Giant"] = [==[%s]==],\n\t\t["Kyrian"] = [==[' % LORE_S)
gp = once(gp, '["VENTHYRHORDE"] = "ORC",', '["VENTHYRHORDE"] = "ORC", ["SLIMEGIANTHORDE"] = "ORC",')
# Centaur (75 Alliance / 102 Horde, Esteria id 68)
cc = once(cc, 'SLIMEGIANTHORDE = 65}', 'SLIMEGIANTHORDE = 65, CENTAURALLIANCE = 68, CENTAURHORDE = 68}')
cc = once(cc, 'CHAR_CREATE_RACE_IDS["Slime Giant"] = {104}', 'CHAR_CREATE_RACE_IDS["Slime Giant"] = {104}' + chr(10) + 'CHAR_CREATE_RACE_IDS["Centaur"] = {75, 102}')
TAB = chr(9)
ICON = 'CHAR_CREATE_EXTRA_RACE_ICONS["Slime Giant"] = "Interface' + chr(92) * 2 + 'Icons' + chr(92) * 2 + 'CoA_Race_SlimeGiant"'
cc = once(cc, TAB + ICON, TAB + ICON + chr(10) + TAB + ICON.replace('Slime Giant', 'Centaur').replace('SlimeGiant', 'Centaur'))
LORE_C = ('Half horse, half warrior, the centaur roam the open plains in clans bound by oath and hoofbeat. Swift, proud '
          'and fiercely free, they ride out of the wilds to see which of the other peoples are worth running beside.')
cc = once(cc, TAB * 2 + '["Slime Giant"] = [==[', TAB * 2 + '["Centaur"] = [==[%s]==],' % LORE_C + chr(10) + TAB * 2 + '["Slime Giant"] = [==[')
gp = once(gp, '["SLIMEGIANTHORDE"] = "ORC",', '["SLIMEGIANTHORDE"] = "ORC", ["CENTAURALLIANCE"] = "HUMAN", ["CENTAURHORDE"] = "ORC",')
# Hozen (112 Alliance, Esteria id 73): the retail Pandaria monkeys, from the PR 31 textures
cc = once(cc, 'CENTAURHORDE = 68}', 'CENTAURHORDE = 68, HOZENALLIANCE = 73}')
cc = once(cc, 'CHAR_CREATE_RACE_IDS["Centaur"] = {75, 102}', 'CHAR_CREATE_RACE_IDS["Centaur"] = {75, 102}' + chr(10) + 'CHAR_CREATE_RACE_IDS["Hozen"] = {112}' + chr(10) +
          'if CHAR_CREATE_MALE_ONLY_RACES then CHAR_CREATE_MALE_ONLY_RACES["Hozen"] = true end')
ICON_C = 'CHAR_CREATE_EXTRA_RACE_ICONS["Centaur"] = "Interface' + chr(92) * 2 + 'Icons' + chr(92) * 2 + 'CoA_Race_Centaur"'
cc = once(cc, TAB + ICON_C, TAB + ICON_C + chr(10) + TAB + ICON_C.replace('Centaur', 'Hozen'))
LORE_H = ('Swift, sharp-tongued and fiercely loyal to their kin, the hozen swing out of the jungles of Pandaria as '
          'traders, scouts and brawlers. Quick to laugh and quicker to pick a fight, they have learned that an '
          'alliance with the other peoples of Azeroth is the best way to keep their families fed and safe.')
cc = once(cc, TAB * 2 + '["Centaur"] = [==[', TAB * 2 + '["Hozen"] = [==[%s]==],' % LORE_H + chr(10) + TAB * 2 + '["Centaur"] = [==[')
gp = once(gp, '["CENTAURHORDE"] = "ORC",', '["CENTAURHORDE"] = "ORC", ["HOZENALLIANCE"] = "HUMAN",')
# Vorkai (110 Alliance, Esteria id 74, retail fauns) and Mantid (106 Horde, Esteria id 75, retail mantid)
cc = once(cc, 'HOZENALLIANCE = 73}', 'HOZENALLIANCE = 73, SYLVARIALLIANCE = 74, MANTIDHORDE = 75, ETHEREALALLIANCE = 76, BANSHEEHORDE = 77, FACELESSHORDE = 78, QIRAJIHORDE = 79}')
cc = once(cc, 'CHAR_CREATE_RACE_IDS["Hozen"] = {112}', 'CHAR_CREATE_RACE_IDS["Hozen"] = {112}' + chr(10) + 'CHAR_CREATE_RACE_IDS["Sylvari"] = {110}' + chr(10) +
          'CHAR_CREATE_RACE_IDS["Mantid"] = {106}' + chr(10) + 'if CHAR_CREATE_MALE_ONLY_RACES then CHAR_CREATE_MALE_ONLY_RACES["Mantid"] = true end' + chr(10) + 'CHAR_CREATE_RACE_IDS["Ethereal"] = {109}' + chr(10) +
          'if CHAR_CREATE_MALE_ONLY_RACES then CHAR_CREATE_MALE_ONLY_RACES["Ethereal"] = true end')
ICON_H = 'CHAR_CREATE_EXTRA_RACE_ICONS["Hozen"] = "Interface' + chr(92) * 2 + 'Icons' + chr(92) * 2 + 'CoA_Race_Hozen"'
cc = once(cc, TAB + ICON_H, TAB + ICON_H + chr(10) + TAB + ICON_H.replace('Hozen', 'Sylvari') + chr(10) + TAB + ICON_H.replace('Hozen', 'Mantid') + chr(10) + TAB + ICON_H.replace('Hozen', 'Ethereal'))
LORE_VK = ('Children of the deep groves, the Sylvari are antlered wanderers whose hooves leave no mark on the moss. '
           'Gentle with the wild and merciless to those who burn it, they walk out of the old forests to stand with '
           'the Alliance and keep the green places from the axe.')
LORE_MT = ('Chitin-shelled and tireless, the mantid swarm out of the thorned fields of the Kun-Lai in endless '
           'broods bound by the will of their hive. A few, driven off by war or called by the Horde, now carry the '
           'ancient claw and the old harvest hunger into the wider world.')
cc = once(cc, TAB * 2 + '["Hozen"] = [==[', TAB * 2 + '["Sylvari"] = [==[%s]==],' % LORE_VK + chr(10) + TAB * 2 + '["Mantid"] = [==[%s]==],' % LORE_MT + chr(10) + TAB * 2 + '["Hozen"] = [==[')
gp = once(gp, '["HOZENALLIANCE"] = "HUMAN",', '["HOZENALLIANCE"] = "HUMAN", ["SYLVARIALLIANCE"] = "HUMAN", ["MANTIDHORDE"] = "ORC",')
LORE_ET = ('Bound in bandages of woven stormlight, the ethereal drift between the worlds as traders of secrets and favours. '
           'Their bodies long since turned to energy, they walk the Alliance\'s roads to learn what the living have forgotten.')
cc = once(cc, TAB * 2 + '["Sylvari"] = [==[', TAB * 2 + '["Ethereal"] = [==[%s]==],' % LORE_ET + chr(10) + TAB * 2 + '["Sylvari"] = [==[')
gp = once(gp, '["MANTIDHORDE"] = "ORC",', '["MANTIDHORDE"] = "ORC", ["ETHEREALALLIANCE"] = "HUMAN",')
# Banshee 90 / Elune's Chosen 118 / Faceless One 87 / Qiraji 88 (native DBC races from the user's RACES_PACK)
PACKR = (('Banshee', 90, 'BansheeHorde', 'Banshee', 'ORC'),
         ('Faceless One', 87, 'FacelessHorde', 'Faceless', 'ORC'))
PACK_LORE = {
    'Banshee': 'Hand-painted spirits of the restless dead, the banshee wail on the wind between the grave and the world of the living. Freed from the Lich King and the Forsaken alike, they carry old sorrows and older grudges to the Horde.',
    "Elune's Chosen": 'Ghostly children of the moon, the Chosen walk between life and starlight, touched by Elune herself. Pale, translucent and patient, they follow the Alliance in the hope that the goddess still has a purpose for them.',
    'Faceless One': 'Servants of the Old Gods, the faceless ones have given up name, face and will to something older than the world. Shadow-drawn and unreadable, they walk with the Horde as guests of the void.',
    'Qiraji': "Born in the sand-choked halls of Ahn'Qiraj, the Qiraji are the swarm walking as individuals: chitin, wings and a hunger that was never theirs. The Horde keeps them close, and keeps a hand on its blade.",
}
for name, rid, fstr, icon, bg in PACKR:
    cc = once(cc, 'CHAR_CREATE_RACE_IDS["Hozen"] = {112}', 'CHAR_CREATE_RACE_IDS["Hozen"] = {112}' + chr(10) + 'CHAR_CREATE_RACE_IDS["%s"] = {%d}' % (name, rid))
    cc = once(cc, TAB + ICON_H, TAB + ICON_H + chr(10) + TAB + 'CHAR_CREATE_EXTRA_RACE_ICONS["%s"] = "Interface' % name + chr(92) * 2 + 'Icons' + chr(92) * 2 + 'CoA_Race_%s"' % icon)
    cc = once(cc, TAB * 2 + '["Hozen"] = [==[', TAB * 2 + '["%s"] = [==[%s]==],' % (name, PACK_LORE[name]) + chr(10) + TAB * 2 + '["Hozen"] = [==[')
    gp = once(gp, '["HOZENALLIANCE"] = "HUMAN",', '["HOZENALLIANCE"] = "HUMAN", ["%s"] = "%s",' % (fstr.upper(), bg))
cc = once(cc, 'CHAR_CREATE_FEMALE_ONLY_RACES = { ["Harpy"] = true }', 'CHAR_CREATE_FEMALE_ONLY_RACES = { ["Harpy"] = true, ["Banshee"] = true }')
cc = once(cc, '["Fel Orc"] = true, ["Skeleton"] = true,', '["Fel Orc"] = true, ["Skeleton"] = true, ["Faceless One"] = true,')
shutil.copytree(r'C:\CoA-Build\slime\out\pack_patch', STAGE, dirs_exist_ok=True)
shutil.copytree(r'C:\CoA-Build\slime\out\pack_dll_root', ROOT, dirs_exist_ok=True)
# compact race grid (three columns of twelve instead of four of ten) and the camera for the oversized added races
cc = once(cc, 'local perColumn, columnWidth, rowHeight, scale = 10, 150, 45, 0.7', 'local perColumn, columnWidth, rowHeight, scale = 14, 128, 34, 0.54')
cc = once(cc, 'CHAR_CREATE_CAMERA_ZOOM_IN = {', 'CHAR_CREATE_CAMERA_ZOOM_IN = { ["FacelessHorde"] = 1.25, ["QirajiHorde"] = 1.3, ["Vrykul"] = 1.3, ["KyrianAlliance"] = 1.3, ["VenthyrHorde"] = 1.5, ["CentaurAlliance"] = 1.2, ["CentaurHorde"] = 1.2, ["HighmountainTauren"] = 1.5,')
# the oversized races (not the Slime Giant) stand further back: the model cannot be moved from Lua, so the stock camera pull-back does it
cc = once(cc, 'CHAR_CREATE_BIG_RACES = { ["Vrykul"] = 1.8 }', 'CHAR_CREATE_BIG_RACES = { ["FacelessHorde"] = 1.5, ["QirajiHorde"] = 1.5, ["Vrykul"] = 1.65, ["KyrianAlliance"] = 1.75, ["VenthyrHorde"] = 1.2, ["CentaurAlliance"] = 1.5, ["CentaurHorde"] = 1.5, ["HighmountainTauren"] = 1.2 }')
# the customise camera of the tall races: the same step back the Highmountain Tauren get (it was filming their legs)
assert cc.count('ZOOM[EsteriaRace() or 0]') == 3
cc = cc.replace('ZOOM[EsteriaRace() or 0]', 'ZOOMK()')
cc = once(cc, 'local ZOOM = {[46] = 1.6}', 'local ZOOM = {[46] = 1.25, [68] = 1.5, [60] = 1.75, [61] = 1.3, [65] = 1.3, VRYKUL = 1.65, FACELESSHORDE = 1.8, QIRAJIHORDE = 1.8}' + chr(10) + chr(9) + 'function ZOOMK()' + chr(10) + chr(9) + chr(9) + 'local id = EsteriaRace()' + chr(10) + chr(9) + chr(9) + 'return (id and ZOOM[id]) or ZOOM[strupper(RaceFile() or "")]' + chr(10) + chr(9) + 'end')
cc += open(r'C:\CoA-Build\slime\out\new_racials.lua', encoding='utf8').read()
cc += open(r'C:\CoA-Build\slime\out\hozen_racials.lua', encoding='utf8').read()
cc += open(r'C:\CoA-Build\slime\out\newraces_racials.lua', encoding='utf8').read()
cc += open(r'C:\CoA-Build\slime\out\active_racials.lua', encoding='utf8').read()
cc += open(r'C:\CoA-Build\slime\out\pack_racials.lua', encoding='utf8').read()
cc += open(r'C:\CoA-Build\slime\out\gallop.lua', encoding='utf8').read()
import race_icons_gender
cc += race_icons_gender.build(put)
cc += '\n-- Local server: the Goblin shows a goblin face like the other races (it showed a boombox item icon)\nCHAR_CREATE_EXTRA_RACE_ICONS["Goblin"] = "Interface\\\\Icons\\\\CoA_Race_AlphaGoblin"\n'
# the race button takes the per-sex icon when there is one (key: race file name upper-cased + sex), else the old single icon
cc = once(cc, '\tlocal iconFile = CHAR_CREATE_EXTRA_RACE_ICONS[raceName]\n',
          '\tlocal iconFile = (CHAR_CREATE_COA_GENDER_ICONS and CHAR_CREATE_COA_GENDER_ICONS[string.upper(raceEnglishName) .. "_" .. gender]) or CHAR_CREATE_EXTRA_RACE_ICONS[raceName]\n')
put('Interface' + BS + 'GlueXML' + BS + 'CharacterCreate.lua', cc.encode('latin-1'))
# the character select stage: the same races are scaled down there
sel, _ = effread.read('Interface' + BS + 'GlueXML' + BS + 'CharacterSelect.lua')
sel = sel.decode('latin-1') + """

-- Local server: the oversized added races are scaled down on the character select stage
do
	local SELECT_SCALE = { ["Kyrian"] = 0.6, ["Centaur"] = 0.6, ["Slime Giant"] = 0.75 }
	local oldUpdateModel = CharacterSelect_UpdateModel
	function CharacterSelect_UpdateModel(self, ...)
		local a, b, c = oldUpdateModel(self, ...)
		local ok, _, race = pcall(GetCharacterInfo, CharacterSelect.selectedCharacterId)
		local k = ok and race and SELECT_SCALE[race]
		if self and self.SetModelScale then
			pcall(self.SetModelScale, self, k or 1)
		end
		return a, b, c
	end
end
"""
put('Interface' + BS + 'GlueXML' + BS + 'CharacterSelect.lua', sel.encode('latin-1'))
put('Interface' + BS + 'GlueXML' + BS + 'GlueParent.lua', gp.encode('latin-1'))

# 5. race button icons cut from the reference renders
d, _ = effread.read('Interface/Icons/CoA_Race_Saberon.blp')
like = blp.read(d)
shots = {'Kyrian': ('kyrian-male-warlock-full.png', (140, 20, 270, 150)),
         'Venthyr': ('venthyr-male-warlock-full.png', (130, 40, 260, 170))}
for race, (f, box) in shots.items():
    im = Image.open(PKG + BS + 'reference-previews' + BS + f).convert('RGBA').crop(box).resize((64, 64), Image.LANCZOS)
    put('Interface' + BS + 'Icons' + BS + 'CoA_Race_%s.blp' % race, blp.encode(im, like))

# 5b. Slime Giant files (build_slime_assets.py)
shutil.copytree(r'C:\CoA-Build\slime\out\patch', STAGE, dirs_exist_ok=True)
shutil.copytree(r'C:\CoA-Build\slime\out\clientroot', ROOT, dirs_exist_ok=True)

# 5c. Centaur files (build_centaur_assets.py) and the icon (the Draenei one until a Centaur portrait exists)
shutil.copytree(r'C:\CoA-Build\slime\out\centaur_patch', STAGE, dirs_exist_ok=True)
shutil.copytree(r'C:\CoA-Build\slime\out\centaur_root', ROOT, dirs_exist_ok=True)
put('Interface' + BS + 'Icons' + BS + 'CoA_Race_Centaur.blp', blp.encode(Image.open(r'C:/CoA-Build/slime/centaur_icon.png').convert('RGBA').resize((64, 64), Image.LANCZOS), blp.read(effread.read('Interface/Icons/CoA_Race_Saberon.blp')[0])))

# 5d. Hozen files (build_hozen_assets.py) and the icon (the head from a render of the retail model)
shutil.copytree(r'C:\CoA-Build\hozen\out\patch', STAGE, dirs_exist_ok=True)
shutil.copytree(r'C:\CoA-Build\hozen\out\clientroot', ROOT, dirs_exist_ok=True)
for race_dir, race_icon, race_png in (('vorkai', 'Sylvari', r'C:/CoA-Build/vorkai/icon_m.png'), ('mantid', 'Mantid', r'C:/CoA-Build/mantid/icon.png')):
    shutil.copytree(r'C:\CoA-Build\%s\out\patch' % race_dir, STAGE, dirs_exist_ok=True)
    shutil.copytree(r'C:\CoA-Build\%s\out\clientroot' % race_dir, ROOT, dirs_exist_ok=True)
    put('Interface' + BS + 'Icons' + BS + 'CoA_Race_%s.blp' % race_icon, blp.encode(Image.open(race_png).convert('RGBA').resize((64, 64), Image.LANCZOS), blp.read(effread.read('Interface/Icons/CoA_Race_Saberon.blp')[0])))
shutil.copytree(r'C:\CoA-Build\ethereal\out_race\patch', STAGE, dirs_exist_ok=True)
shutil.copytree(r'C:\CoA-Build\ethereal\out_race\clientroot', ROOT, dirs_exist_ok=True)
put('Interface' + BS + 'Icons' + BS + 'CoA_Race_Ethereal.blp', blp.encode(Image.open(r'C:/CoA-Build/ethereal/icon.png').convert('RGBA').resize((64, 64), Image.LANCZOS), blp.read(effread.read('Interface/Icons/CoA_Race_Saberon.blp')[0])))
put('Interface' + BS + 'Icons' + BS + 'CoA_Race_Hozen.blp', blp.encode(Image.open(r'C:/CoA-Build/hozen/hozen_icon_crop.png').convert('RGBA').resize((64, 64), Image.LANCZOS), blp.read(effread.read('Interface/Icons/CoA_Race_Saberon.blp')[0])))

# 6. archive
S = storm.S
files = []
for d0, _, names in os.walk(STAGE):
    for n in names:
        disk = os.path.join(d0, n)
        files.append((os.path.relpath(disk, STAGE).replace('/', BS), disk))
S.SFileCreateArchive.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(ctypes.c_void_p)]
S.SFileCreateArchive.restype = ctypes.c_bool
S.SFileAddFileEx.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
S.SFileAddFileEx.restype = ctypes.c_bool
if os.path.exists(ARCHIVE):
    os.remove(ARCHIVE)
h = ctypes.c_void_p()
assert S.SFileCreateArchive(ARCHIVE.encode(), 0x01000000 | 0x00100000 | 0x00200000, len(files) + 64, ctypes.byref(h))
for rel, disk in files:
    assert S.SFileAddFileEx(h, os.path.abspath(disk).encode(), rel.encode(), 0x200 | 0x80000000, 2, 2), rel
S.SFileCloseArchive(h)
v = mpqread.MPQ(ARCHIVE)
for rel, disk in files:
    assert v.read(rel) == open(disk, 'rb').read(), rel
print('built and verified', ARCHIVE, len(files), 'files', os.path.getsize(ARCHIVE), 'bytes')
