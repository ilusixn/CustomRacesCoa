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
for race_dir, race_icon, race_png in (('vorkai', 'Sylvari', r'C:/CoA-Build/vorkai/icon_m.png'), ('mantid', 'Mantid', r'C:/CoA-Build/mantid/icon.png')):
    shutil.copytree(r'C:\CoA-Build\%s\out\patch' % race_dir, STAGE, dirs_exist_ok=True)
    shutil.copytree(r'C:\CoA-Build\%s\out\clientroot' % race_dir, ROOT, dirs_exist_ok=True)
    put('Interface' + BS + 'Icons' + BS + 'CoA_Race_%s.blp' % race_icon, blp.encode(Image.open(race_png).convert('RGBA').resize((64, 64), Image.LANCZOS), blp.read(effread.read('Interface/Icons/CoA_Race_Saberon.blp')[0])))
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
