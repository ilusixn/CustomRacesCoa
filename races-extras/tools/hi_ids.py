import struct, collections
base = r'C:\Users\ilusi\Downloads\Kyrian-Venthyr-Approved-20261021\Kyrian-Venthyr-Approved-20261021\editable-assets\custom'
for p in (r'\kyrian\native\kyrianmale00.skin', r'\venthyr\native\vampire_male00.skin'):
    s = open(base + p, 'rb').read()
    magic, ni, oi, nt, ot, np_, op, nsub, osub, ntu, otu, bones = struct.unpack_from('<4sIIIIIIIIIII', s, 0)
    c = collections.Counter()
    for i in range(nsub):
        sid, lvl, sv, nv, st, nti = struct.unpack_from('<6H', s, osub + i * 48)
        if sid >= 1300:
            c[sid] += nti
    print(p, dict(sorted(c.items())))
