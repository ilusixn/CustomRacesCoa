"""Append the Aetheria Kyrian / Venthyr body skins that the cached bank (EsteriaKvSkins.bin) lacks: every kind-1 type-11 path of the
installed EsteriaKyrian.bin / EsteriaVenthyr.bin that is not in the bank yet is read from the unpacked Aetheria assets, decoded and added.
The bank is rewritten in place (backup next to it)."""
import ctypes, os, shutil, struct, sys
import numpy as np
BS = chr(92)
ROOT = r'C:\Ascension Local'
BANK = ROOT + BS + 'EsteriaKvSkins.bin'
UNP = r'C:\CoA-Build\sources\shotboom\unpacked'
exec(open(r'C:\CoA-Build\kv\build_data.py').read().split("def decode_blp")[0].split("import effread")[0].replace("sys.path[:0]", "pass #"), {}) if False else None


def dxt_decode(data, w, h, fmt):
    bw, bh = w // 4, h // 4
    size = 8 if fmt == 1 else 16
    blocks = np.frombuffer(data[:bw * bh * size], np.uint8).reshape(bh * bw, size)
    cb = blocks[:, -8:]
    c0 = cb[:, 0].astype(np.uint32) | (cb[:, 1].astype(np.uint32) << 8)
    c1 = cb[:, 2].astype(np.uint32) | (cb[:, 3].astype(np.uint32) << 8)
    def rgb(c):
        return np.stack([((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31], 1)
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
            six = ((6 - i) * a0 + (i - 1) * a1) // 5 if i < 6 else np.full_like(a0, 0 if i == 6 else 255)
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
    assert ntdll.RtlCompressBuffer(2, buf, len(buf), out, len(out), 4096, ctypes.byref(final), work) == 0
    return out.raw[:final.value]


data = open(BANK, 'rb').read()
count = struct.unpack_from('<I', data, 4)[0]
have, pos = set(), 8
for i in range(count):
    name = data[pos:pos + 128].split(b'\0')[0].decode()
    csize = struct.unpack_from('<I', data, pos + 136)[0]
    have.add(name.lower())
    pos += 144 + csize
assert pos == len(data), (pos, len(data))
need = []
for f in ('EsteriaKyrian.bin', 'EsteriaVenthyr.bin'):
    d = open(ROOT + BS + f, 'rb').read()
    n = struct.unpack_from('<I', d, 8)[0]
    for i in range(n):
        g, k, t = struct.unpack_from('<3I', d, 12 + i * 156)
        if k == 1 and t == 11:
            p = d[12 + i * 156 + 28:12 + i * 156 + 156].split(b'\0')[0].decode()
            if p.lower() not in have and p.lower() not in need:
                need.append(p.lower())
print('bank has', count, 'skins; missing', len(need))
add, missing = b'', []
for p in need:
    path = UNP + BS + p.replace('/', BS)
    if not os.path.exists(path):
        missing.append(p)
        continue
    rgba, w, h = decode_blp(open(path, 'rb').read())
    z = lznt1(rgba.tobytes())
    add += p.encode().ljust(128, b'\0') + struct.pack('<4I', w, h, len(z), 0) + z
print('not found in the unpacked assets:', missing[:10], len(missing))
shutil.copy2(BANK, BANK + '.before_aetheria')
open(BANK, 'wb').write(b'KVS1' + struct.pack('<I', count + len(need) - len(missing)) + data[8:] + add)
print('written', os.path.getsize(BANK))
