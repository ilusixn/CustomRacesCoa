"""Seed the Retail adapter's decoded cache with the appearance DB2s (CDN reads of the encrypted ones fail), taken raw from wago.tools."""
import os, struct, sys, urllib.request
ROOT = r'C:\Ascension Local\Data\Retail'
BUILD = 'd718abcc42604d8efbd35aecfa5bb75b'
names = ['charcomponenttexturelayouts', 'charcomponenttexturesections', 'chrcustomizationchoice', 'chrcustomizationcondmodel',
         'chrcustomizationelement', 'chrcustomizationgeoset', 'chrcustomizationmaterial', 'chrcustomizationoption',
         'chrcustomizationskinnedmodel', 'chrmodel', 'chrmodeltexturelayer', 'chrracexchrmodel', 'creaturedisplayinfo',
         'creaturedisplayinfogeosetdata', 'creaturemodeldata', 'texturefiledata']
ids = {}
for line in open(os.path.join(ROOT, 'community-listfile.csv'), encoding='utf-8'):
    i, _, p = line.rstrip('\n').partition(';')
    if p.startswith('dbfilesclient/') and p.endswith('.db2') and p[14:-4] in names:
        ids[p[14:-4]] = int(i)
def fnv(b):
    h = 14695981039346656037
    for x in b:
        h ^= x; h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h
fp = fnv(open(os.path.join(ROOT, 'keys.txt'), 'rb').read())
out = os.path.join(ROOT, 'Cache', 'Retail', 'wow-' + BUILD, 'decoded', '2', 'tables-%d' % fp)
os.makedirs(out, exist_ok=True)
for n in names:
    fid = ids[n]
    data = urllib.request.urlopen(urllib.request.Request('https://wago.tools/api/casc/%d?download&build=12.1.5.70575' % fid, headers={'User-Agent': 'curl/8'})).read()
    assert data[:4] == b'WDC5', (n, data[:8])
    open(os.path.join(out, 'id-%d.bin' % fid), 'wb').write(struct.pack('<IIQ', 0x31434457, len(data), fnv(data)) + data)
    print(n, fid, len(data))
