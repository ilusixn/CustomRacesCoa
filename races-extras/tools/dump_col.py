import sys
sys.path[:0] = [r'C:\CoA-Build\esteria_racials', r'C:\CoA-Build\tools']
import estlib
B = chr(92)
cl = estlib.Client(estlib.OUR_DATA)
for n in ('Collections.lua', 'CollectionsTabMixin.lua', 'Collections.xml', 'Ascension_Collections.toc'):
    d, p = cl.read('Interface' + B + 'AddOns' + B + 'Ascension_Collections' + B + n)
    open(r'C:\CoA-Build\kv\col_' + n, 'wb').write(d)
    print(n, len(d), p)
