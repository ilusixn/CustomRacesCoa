import struct, glob
base = r'C:\Users\ilusi\Downloads\Kyrian-Venthyr-Approved-20261021\Kyrian-Venthyr-Approved-20261021\editable-assets\custom'
for f in glob.glob(base + '/*/native/*00.skin'):
    d = open(f, 'rb').read()
    magic, ni, oi, nt, ot, np_, op, ns, os_, ntu, otu, bones = struct.unpack_from('<4sIIIIIIIIIII', d, 0)
    tot = 0
    mx = 0
    hide = 0
    for i in range(ns):
        sid, lvl, sv, nv, st, nti = struct.unpack_from('<6H', d, os_ + i * 48)
        tot += nti
        mx = max(mx, nti)
    print(f.split('custom')[-1], 'indices', ni, 'tris(list)', nt, 'submeshes', ns, 'sum submesh idx', tot, 'max', mx, 'texunits', ntu, 'levels?')
