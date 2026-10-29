"""Build CoA Custom 1.9.3 (x64 client) from the 1.9.2 package + the live client/server files.
Server zip: 1.9.2 Server + new worldserver.exe, SQL 16, notes.  Client-6: only what is new or changed since Client-1..5
(race archives, CoA64.exe, its Retail data)."""
import binascii, hashlib, json, os, shutil, zipfile

SRC = r'C:\CoA-Build\release192\out'
OUT = r'C:\CoA-Build\release193\out'
NOTES = r'C:\CoA-Build\release193'
STAGE_S = r'C:\CoA-Build\release193\stage_s'
STAGE_C = r'C:\CoA-Build\release193\stage_c6'
GAME = r'C:\Ascension Local'
WS = r'C:\CoA-Repack\CoA-Bots\Core\worldserver.exe'
DBC = r'C:\CoA-Repack\Data\dbc'
V = '1.9.3'
CASC = 'wow-d718abcc42604d8efbd35aecfa5bb75b'


def crc(p):
    c = 0
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 24), b''):
            c = binascii.crc32(b, c)
    return c


def zip_dir(root, path):
    if os.path.exists(path):
        os.remove(path)
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        for d, _, files in os.walk(root):
            for f in files:
                p = os.path.join(d, f)
                z.write(p, os.path.relpath(p, root))
    print('wrote', path, os.path.getsize(path) // 2 ** 20, 'MB')


os.makedirs(OUT, exist_ok=True)
old = {}                                                  # published path (below CoA-Custom/files/) -> CRC in 1.9.2
for n in range(1, 6):
    src = os.path.join(SRC, 'CoA-Custom-1.9.2-Client-%d.zip' % n)
    with zipfile.ZipFile(src) as z:
        for i in z.infolist():
            if '/files/' in i.filename and not i.is_dir():
                old[i.filename.split('/files/', 1)[1]] = i.CRC
    dst = os.path.join(OUT, 'CoA-Custom-%s-Client-%d.zip' % (V, n))
    if os.path.exists(dst):
        os.remove(dst)
    try:
        os.link(src, dst)
    except OSError:
        shutil.copy2(src, dst)

# ---- Client-6
shutil.rmtree(STAGE_C, ignore_errors=True)
files = os.path.join(STAGE_C, 'CoA-Custom', 'files')
added = []


def put(src, rel):
    if rel in old and old[rel] == crc(src):
        return
    dst = os.path.join(files, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)
    added.append(rel)


data = os.path.join(GAME, 'Data')
for f in sorted(os.listdir(data)):
    if f.startswith('patch-Z') and f.endswith('.MPQ') and f != 'patch-Z.MPQ':      # patch-Z is the base client's own archive
        put(os.path.join(data, f), 'client/' + f)
for f in sorted(os.listdir(GAME)):
    if ((f.startswith('Esteria') and f.endswith('.bin')) or f == 'EsteriaAppearance.dll') and 'Qiraji' not in f:   # Qiraji is shelved
        put(os.path.join(GAME, f), 'client_root/' + f)
for f in ('CoA64.exe', 'XvidDecoder.dll'):
    put(os.path.join(GAME, f), 'client_root/' + f)
retail = os.path.join(data, 'Retail')
for f in ('RetailCasc.ini', 'RetailRaces.csv', 'keys.txt', 'community-listfile.csv'):
    put(os.path.join(retail, f), 'client_root/Data/Retail/' + f)
for d, _, fs in os.walk(os.path.join(retail, 'Assets')):
    for f in fs:
        p = os.path.join(d, f)
        put(p, 'client_root/Data/Retail/Assets/' + os.path.relpath(p, os.path.join(retail, 'Assets')).replace(chr(92), '/'))
seed = os.path.join(retail, 'Cache', 'Retail', CASC, 'decoded', '2')        # the appearance tables the CDN cannot give (encrypted)
for d, _, fs in os.walk(seed):
    if os.path.basename(d).startswith('tables-'):
        for f in fs:
            p = os.path.join(d, f)
            put(p, 'client_root/Data/Retail/Cache/Retail/%s/decoded/2/%s/%s' % (CASC, os.path.basename(d), f))
print('client-6 files:', len(added))
for a in added:
    print('  ', a)
zip_dir(STAGE_C, os.path.join(OUT, 'CoA-Custom-%s-Client-6.zip' % V))

# ---- Server
shutil.rmtree(STAGE_S, ignore_errors=True)
os.makedirs(STAGE_S)
with zipfile.ZipFile(os.path.join(SRC, 'CoA-Custom-1.9.2-Server.zip')) as z:
    z.extractall(STAGE_S)
root = os.path.join(STAGE_S, 'CoA-Custom')
for f in ('INSTALL-GUIDE.txt', 'PATCH-NOTES-1.9.3.txt'):
    shutil.copy2(os.path.join(NOTES, f), os.path.join(root, f))
shutil.copy2(os.path.join(NOTES, '16_1.9.3_x64.sql'), os.path.join(root, 'files', 'sql', '16_1.9.3_x64.sql'))
man = os.path.join(root, 'manifest.json')
text = open(man, encoding='utf-8').read().replace('"version": "1.9.2"', '"version": "%s"' % V).replace('"built": "2026-10-26"', '"built": "2026-10-29"')
open(man, 'w', encoding='utf-8').write(text)
shutil.copy2(WS, os.path.join(root, 'files', 'worldserver.exe'))
for name in ('ChrRaces.dbc', 'Faction.dbc', 'UICameraAppearanceChrRaces.dbc', 'EmotesTextSound.dbc', 'CreatureDisplayInfo.dbc', 'Spell.dbc'):
    s = os.path.join(DBC, name)
    if os.path.isfile(s):
        shutil.copy2(s, os.path.join(root, 'files', 'dbc', name))
cdi = os.path.join(DBC, 'CreatureDisplayInfo.dbc')
if os.path.isfile(cdi):
    os.makedirs(os.path.join(root, 'files', 'dbc_clientset'), exist_ok=True)
    shutil.copy2(cdi, os.path.join(root, 'files', 'dbc_clientset', 'CreatureDisplayInfo.dbc'))
zip_dir(STAGE_S, os.path.join(OUT, 'CoA-Custom-%s-Server.zip' % V))
for f in ('INSTALL-GUIDE.txt', 'PATCH-NOTES-1.9.3.txt'):
    shutil.copy2(os.path.join(NOTES, f), os.path.join(OUT, f))

sums = {}
for f in sorted(os.listdir(OUT)):
    if f.endswith('.zip'):
        h = hashlib.sha256()
        with open(os.path.join(OUT, f), 'rb') as fh:
            for b in iter(lambda: fh.read(1 << 24), b''):
                h.update(b)
        sums[f] = h.hexdigest()
open(os.path.join(OUT, 'SHA256SUMS.txt'), 'w').write(''.join('%s  %s\n' % (v, k) for k, v in sums.items()))
print('done')
