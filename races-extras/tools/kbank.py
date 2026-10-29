import struct, collections, sys
base = r'C:\Users\ilusi\Downloads\Kyrian-Venthyr-Approved-20261021\Kyrian-Venthyr-Approved-20261021'
d = open(base + r'\client\Data\Retail\EsteriaKyrian.bin', 'rb').read()
n = struct.unpack_from('<I', d, 8)[0]
for i in range(n):
    g, k, t, v, c0, c1, c2 = struct.unpack_from('<7I', d, 12 + i * 156)
    if k != 0:
        continue
    ch = [(c & 0xffff, c >> 16) for c in (c0, c1, c2) if c != 0xffffffff]
    print('sex', g, 'group', t, 'geoset', v, 'when', ch)
# skins
for name in ('kyrianmale', 'kyrianfemale'):
    s = open(base + r'\editable-assets\custom\kyrian\native\%s00.skin' % name, 'rb').read()
    magic, ni, oi, nt, ot, np_, op, nsub, osub, ntu, otu, bones = struct.unpack_from('<4sIIIIIIIIIII', s, 0)
    groups = collections.defaultdict(lambda: [0, 0])
    for i in range(nsub):
        sid, lvl, sv, nv, st, nti = struct.unpack_from('<6H', s, osub + i * 48)
        groups[sid // 100][0] += 1
        groups[sid // 100][1] += nti
    print(name, 'total', sum(v[1] for v in groups.values()), {g: tuple(v) for g, v in sorted(groups.items())})
# attachments
for path in (r'\editable-assets\custom\kyrian\native\kyrianmale.m2', r'\editable-assets\custom\kyrian\native\kyrianfemale.m2',
             r'\editable-assets\custom\venthyr\native\vampire_male.m2', r'\editable-assets\custom\venthyr\native\vampire_female.m2'):
    m = open(base + path, 'rb').read()
    na, oa = struct.unpack_from('<II', m, 0xF0)
    nl, ol = struct.unpack_from('<II', m, 0xF8)
    ids = [struct.unpack_from('<I', m, oa + i * 40)[0] for i in range(na)]
    lk = [struct.unpack_from('<H', m, ol + 2 * i)[0] for i in range(nl)]
    print(path.split('\\')[-1], 'attachments', ids, 'lookup', lk)
