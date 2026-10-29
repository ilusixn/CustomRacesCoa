import struct, sys, collections
base = r'C:\Users\ilusi\Downloads\Kyrian-Venthyr-Approved-20261021\Kyrian-Venthyr-Approved-20261021\client\Data\Retail'
for name in ('EsteriaKyrian.bin', 'EsteriaVenthyr.bin'):
    d = open(base + '\\' + name, 'rb').read()
    magic, ver, n = struct.unpack_from('<4sII', d, 0)
    print(name, magic, ver, n, len(d), 12 + n * 156)
    kinds = collections.Counter()
    bad = 0
    for i in range(n):
        g, k, t, v, c0, c1, c2 = struct.unpack_from('<7I', d, 12 + i * 156)
        path = d[12 + i * 156 + 28:12 + i * 156 + 156].split(b'\0')[0].decode()
        kinds[(k, g)] += 1
        ok = g < 2 and k <= 5
        if ok:
            if k == 5:
                ok = t < 16 and v and path
            elif k == 4:
                ok = t and v < 52
            elif k == 3:
                ok = t < 9 and v >= 16
            elif k == 2:
                ok = t < 5200 and 10000 <= v < 30000
            elif k == 1:
                ok = t < 16 and path
            else:
                ok = t < 52 and t * 100 <= v < t * 100 + 100
        for c in (c0, c1, c2):
            ok = ok and (c == 0xffffffff or ((c & 0xffff) < 32 and (c >> 16) < 256))
        if not ok:
            bad += 1
            print('  BAD', i, g, k, t, v, hex(c0), hex(c1), hex(c2), path)
        if i < 6 or k in (1, 5) and i < 30:
            print('  rec', i, 'g', g, 'k', k, 't', t, 'v', v, hex(c0), hex(c1), hex(c2), path)
    print(' kinds (kind,gender):', dict(kinds), 'bad', bad)
