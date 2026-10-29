"""Data files for the Kyrian / Venthyr armor port (read by KvOutfits.inl in the DLL).
   EsteriaKvSkins.bin : KVS1, count, then {char path[128]; u32 w, h, csize; LZNT1(RGBA)}: the type-11 body skins and the
                        Venthyr bare-leg skins, already decoded (the 32-bit client cannot read 2.7 MB layers).
   EsteriaKvItems.bin : KVI1, count, then {u32 id; i32 geosetGroup[3]} of every ItemDisplayInfo row that has one.
   Data/Retail/Assets/EquipmentFitting/{60,61}-{0,1}.bin : the package's UV fitting maps, copied."""
import ctypes, os, shutil, struct, sys
import numpy as np
sys.path[:0] = [r'C:\CoA-Build\kv', r'C:\CoA-Build\eunoia', r'C:\CoA-Build\tools', r'C:\CoA-Build\esteria_racials']
import effread, estlib, mpqread

BS = chr(92)
PKG = r'C:\Users\ilusi\Downloads\Kyrian-Venthyr-Approved-20261021\Kyrian-Venthyr-Approved-20261021'
OUT = r'C:\CoA-Build\kv\out\clientroot'
os.makedirs(OUT, exist_ok=True)
assets = mpqread.MPQ(PKG + BS + 'client' + BS + 'race-assets.mpq')


def dxt_decode(data, w, h, fmt):
    bw, bh = w // 4, h // 4
    size = 8 if fmt == 1 else 16
    blocks = np.frombuffer(data[:bw * bh * size], np.uint8).reshape(bh * bw, size)
    cb = blocks[:, -8:]
    c0 = cb[:, 0].astype(np.uint32) | (cb[:, 1].astype(np.uint32) << 8)
    c1 = cb[:, 2].astype(np.uint32) | (cb[:, 3].astype(np.uint32) << 8)

    def rgb(c):
        r = ((c >> 11) & 31) * 255 // 31
        g = ((c >> 5) & 63) * 255 // 63
        b = (c & 31) * 255 // 31
        return np.stack([r, g, b], 1)

    p0, p1 = rgb(c0), rgb(c1)
    four = (c0 > c1)[:, None]
    palette = np.zeros((len(blocks), 4, 3), np.uint32)
    palette[:, 0], palette[:, 1] = p0, p1
    palette[:, 2] = np.where(four, (2 * p0 + p1) // 3, (p0 + p1) // 2)
    palette[:, 3] = np.where(four, (p0 + 2 * p1) // 3, 0)
    bits = cb[:, 4].astype(np.uint32) | (cb[:, 5].astype(np.uint32) << 8) | (cb[:, 6].astype(np.uint32) << 16) | (cb[:, 7].astype(np.uint32) << 24)
    idx = (bits[:, None] >> (2 * np.arange(16))[None, :]) & 3
    color = np.take_along_axis(palette, idx[:, :, None].repeat(3, 2), 1)
    alpha = np.full((len(blocks), 16), 255, np.uint32)
    if fmt == 5:
        a0, a1 = blocks[:, 0].astype(np.int64), blocks[:, 1].astype(np.int64)
        ap = np.zeros((len(blocks), 8), np.int64)
        ap[:, 0], ap[:, 1] = a0, a1
        gt = a0 > a1
        for i in range(2, 8):
            eight = ((8 - i) * a0 + (i - 1) * a1) // 7
            if i < 6:
                six = ((6 - i) * a0 + (i - 1) * a1) // 5
            else:
                six = np.full_like(a0, 0 if i == 6 else 255)
            ap[:, i] = np.where(gt, eight, six)
        ab = np.zeros(len(blocks), np.uint64)
        for i in range(6):
            ab |= blocks[:, 2 + i].astype(np.uint64) << np.uint64(8 * i)
        ai = ((ab[:, None] >> (3 * np.arange(16).astype(np.uint64))[None, :]) & np.uint64(7)).astype(np.int64)
        alpha = np.take_along_axis(ap, ai, 1)
    elif fmt == 3:
        ab = np.zeros(len(blocks), np.uint64)
        for i in range(8):
            ab |= blocks[:, i].astype(np.uint64) << np.uint64(8 * i)
        alpha = (((ab[:, None] >> (4 * np.arange(16).astype(np.uint64))[None, :]) & np.uint64(15)) * 17).astype(np.uint32)
    px = np.concatenate([color, alpha[:, :, None]], 2).reshape(bh, bw, 4, 4, 4)
    return px.transpose(0, 2, 1, 3, 4).reshape(h, w, 4).astype(np.uint8)


def decode_blp(d):
    magic, typ, enc, ad, ae, mips, w, h = struct.unpack_from('<4sIBBBBII', d, 0)
    off = struct.unpack_from('<I', d, 20)[0]
    size = struct.unpack_from('<I', d, 84)[0]
    raw = d[off:off + size]
    if enc == 3:
        a = np.frombuffer(raw, np.uint8).reshape(h, w, 4)
        return a[..., [2, 1, 0, 3]].copy(), w, h
    assert enc == 2, enc
    return dxt_decode(raw, w, h, 1 if ae == 0 else 3 if ae == 1 else 5), w, h


def lznt1(buf):
    ntdll = ctypes.WinDLL('ntdll')
    ws, frag = ctypes.c_ulong(), ctypes.c_ulong()
    ntdll.RtlGetCompressionWorkSpaceSize(2, ctypes.byref(ws), ctypes.byref(frag))
    work = ctypes.create_string_buffer(ws.value)
    out = ctypes.create_string_buffer(len(buf) + 4096)
    final = ctypes.c_ulong()
    st = ntdll.RtlCompressBuffer(2, buf, len(buf), out, len(out), 4096, ctypes.byref(final), work)
    assert st == 0, st
    return out.raw[:final.value]


# skins: every kind-1 type-11 path of the banks, plus the Venthyr bare-leg skins
paths = []
for name in ('EsteriaKyrian.bin', 'EsteriaVenthyr.bin'):
    d = open(PKG + BS + 'client' + BS + 'Data' + BS + 'Retail' + BS + name, 'rb').read()
    n = struct.unpack_from('<I', d, 8)[0]
    for i in range(n):
        g, k, t = struct.unpack_from('<3I', d, 12 + i * 156)
        if k == 1 and t == 11:
            p = d[12 + i * 156 + 28:12 + i * 156 + 156].split(b'\0')[0].decode()
            if p not in paths:
                paths.append(p)
for n in assets.listfile():
    if n.lower().startswith('custom' + BS + 'equipment' + BS + 'native' + BS + 'legskin-v3-61'):
        paths.append(n)
SLIME = []
for c in range(35):
    if c < 5:
        SLIME.append(('custom' + BS + 'slimegiant' + BS + 'unknown' + BS + '%d.blp' % (3491663 + 2 * c),
                      r'C:\CoA-Build\slime\out\tex_fixed\%d.blp' % (3491663 + 2 * c)))
    else:
        SLIME.append(('custom' + BS + 'slimegiant' + BS + 'colors' + BS + '%d.blp' % (2 * (c - 5)),
                      r'C:\CoA-Build\slime\out\patch\custom\slimegiant\colors\%d.blp' % (2 * (c - 5))))
CENTAUR_SKINS = [[4036659, 4036672, 4036678, 4036679, 4036680, 4036681, 4036682, 4036683,
                  4036685, 4036686, 4036687, 4036688, 4036689, 4036690, 4036691, 4036692],
                 [4081391, 4081390, 4081392, 4081393, 4081394, 4081395, 4081396, 4081397,
                  4081399, 4081400, 4081401, 4081402, 4081403, 4081404, 4081405, 4081406]]
for sex in CENTAUR_SKINS:
    for n in sex:
        SLIME.append(('custom' + BS + 'centaur' + BS + 'unknown' + BS + '%d.blp' % n,
                      r'C:\CoA-Build\slime\out\centaur_tex\unknown\%d.blp' % n))
SLIME.append(('custom' + BS + 'centaur' + BS + 'unknown' + BS + '4081389.blp',      # the female's second texture (her top is drawn with it)
              r'C:\CoA-Build\slime\out\centaur_tex\unknown\4081389.blp'))
for c in range(18):
    SLIME.append(('custom' + BS + 'hozen' + BS + 'unknown' + BS + 'hozen_fur_%d.blp' % c,
                  r'C:\CoA-Build\hozen\out\patch\custom\hozen\unknown\hozen_fur_%d.blp' % c))
for folder, root in (('vorkai' + BS + 'fmm', 'vorkai'), ('vorkai' + BS + 'fnf00', 'vorkai'), ('mantid' + BS + 'mt00', 'mantid')):
    for c in range(8):
        SLIME.append(('custom' + BS + folder + BS + 'fur_%d.blp' % c,
                      'C:' + BS + 'CoA-Build' + BS + root + BS + 'out' + BS + 'patch' + BS + 'custom' + BS + folder + BS + 'fur_%d.blp' % c))
for c in range(8):
    SLIME.append(('custom' + BS + 'ethereal' + BS + 'e' + BS + 'fur_%d.blp' % c,
                  'C:' + BS + 'CoA-Build' + BS + 'ethereal' + BS + 'out_race' + BS + 'patch' + BS + 'custom' + BS + 'ethereal' + BS + 'e' + BS + 'fur_%d.blp' % c))
for n in range(10):
    SLIME.append(('custom' + BS + 'hozen' + BS + 'unknown' + BS + 'mark_%d.blp' % n, r'C:\CoA-Build\hozen\out\marks\mark_%d.blp' % n))
for bank in ('EsteriaBanshee.bin', 'EsteriaFaceless.bin'):
    d = open(r'C:\CoA-Build\slime\out\pack_dll_root' + BS + bank, 'rb').read()
    for i in range(struct.unpack_from('<I', d, 8)[0]):
        o = 12 + i * 156
        g, k, t = struct.unpack_from('<3I', d, o)
        rel = d[o + 28:o + 156].split(bytes(1))[0].decode()
        if g == 0 and k == 1 and t == 11 and rel.lower() not in [x[0].lower() for x in SLIME]:
            if not os.path.exists(r'C:\CoA-Build\slime\out\pack_patch' + BS + rel):
                SLIME.append((rel, None))
                continue
            SLIME.append((rel, r'C:\CoA-Build\slime\out\pack_patch' + BS + rel))
body = b''
for p in paths:
    rgba, w, h = decode_blp(assets.read(p))
    z = lznt1(rgba.tobytes())
    body += p.lower().encode().ljust(128, b'\0') + struct.pack('<4I', w, h, len(z), 0) + z
def decode_any(data):
    try:
        return decode_blp(data)
    except AssertionError:
        import io
        from PIL import Image
        import numpy as np
        magic, typ, enc, ad, ae, mips, w, h = struct.unpack_from('<4sIBBBBII', data, 0)
        if enc == 1:        # palettised: Pillow drops the alpha plane, read it here
            off = struct.unpack_from('<I', data, 20)[0]
            pal = np.frombuffer(data, np.uint8, 1024, 148).reshape(256, 4)
            idx = np.frombuffer(data, np.uint8, w * h, off)
            rgba = np.empty((w * h, 4), np.uint8)
            rgba[:, 0] = pal[idx, 2]; rgba[:, 1] = pal[idx, 1]; rgba[:, 2] = pal[idx, 0]
            rgba[:, 3] = np.frombuffer(data, np.uint8, w * h, off + w * h) if ad == 8 else 255
            return rgba.reshape(h, w, 4), w, h
        im = Image.open(io.BytesIO(data)).convert('RGBA')
        return np.asarray(im), im.size[0], im.size[1]


for name, disk in SLIME:
    rgba, w, h = decode_any(effread.read(name)[0] if disk is None else open(disk, 'rb').read())
    z = lznt1(rgba.tobytes())
    body += name.lower().encode().ljust(128, b'\0') + struct.pack('<4I', w, h, len(z), 0) + z
open(OUT + BS + 'EsteriaKvSkins.bin', 'wb').write(b'KVS1' + struct.pack('<I', len(paths) + len(SLIME)) + body)
print('skins', len(paths) + len(SLIME), len(body))

# items: geoset groups of ItemDisplayInfo
d, p = effread.read('DBFilesClient' + BS + 'ItemDisplayInfo.dbc')
T = estlib.DBC(d)
sg = lambda v: v - (1 << 32) if v >= (1 << 31) else v
rows = [(r[0], sg(r[7]), sg(r[8]), sg(r[9])) for r in T.rows if r[7] or r[8] or r[9]]
rows.sort()
open(OUT + BS + 'EsteriaKvItems.bin', 'wb').write(b'KVI1' + struct.pack('<I', len(rows)) + b''.join(struct.pack('<Iiii', *r) for r in rows))
print('items', len(rows), 'from', p.split(BS)[-1])

# fitting maps
dst = OUT + BS + 'Data' + BS + 'Retail' + BS + 'Assets' + BS + 'EquipmentFitting'
os.makedirs(dst, exist_ok=True)
for f in os.listdir(PKG + BS + 'client' + BS + 'Data' + BS + 'Retail' + BS + 'Assets' + BS + 'EquipmentFitting'):
    shutil.copy2(PKG + BS + 'client' + BS + 'Data' + BS + 'Retail' + BS + 'Assets' + BS + 'EquipmentFitting' + BS + f, dst)
print('maps', os.listdir(dst))

for name in ('65-0.bin', '68-0.bin', '68-1.bin', '1068-1.bin', '73-0.bin', '74-0.bin', '74-1.bin', '75-0.bin', '76-0.bin', '77-0.bin', '78-0.bin'):
    shutil.copy2(r'C:/CoA-Build/slime/out/fit/' + name, dst)
print('slime fitting map copied')
