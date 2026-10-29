"""Venthyr without the built-in outfits (body styles other than the basic 3101, collars, headwear): the full set made the
skins 160k-198k triangle indices, which breaks the client's shadow pass. Armor goes through the equipment geosets."""
import struct

LEGACY_BODY = {20200}
KYRIAN_HIDE = {3201, 3202, 3203, 3204, 3301, 3302, 3303, 3401, 3402, 3403}   # headwear, outfits, shoulders
HIDE_MALE = {3102, 3103, 3104, 3105, 3106, 3402, 3403, 3404, 3405, 3406, 3502, 3503} | LEGACY_BODY | {1302, 20104}  # the long skirt (robe) and the wide trouser flare are gone from the model
HIDE_FEMALE = {3102, 3103, 3104, 3402, 3403, 3404, 3405, 3406, 3502, 3503, 3504, 3505, 3506, 3507, 3508, 3509} | LEGACY_BODY | {1302, 20104}
# bank: (gender, group) -> only the first (none / basic) record stays; the control values above 0 are gone
DROP = {(0, 31), (0, 34), (0, 35), (1, 31), (1, 34), (1, 35)}
KYRIAN_DROP = {(0, 32), (0, 33), (0, 34), (1, 32), (1, 33), (1, 34)}


def trim_skin(skin, hide):
    skin = bytearray(skin)
    ni, oi, nt, ot = struct.unpack_from('<IIII', skin, 4)
    ns, os_ = struct.unpack_from('<II', skin, 28)
    tri = struct.unpack_from('<%dH' % nt, skin, ot)
    subs = []
    for i in range(ns):
        sid, lvl, sv, nv, st, nti = struct.unpack_from('<6H', skin, os_ + i * 48)
        subs.append([i, sid, st + (lvl << 16), nti])
    keep = [s for s in subs if s[1] not in hide and s[3]]
    big = max(keep, key=lambda s: s[3])
    order = [s for s in keep if s is not big] + [big]
    out = []
    for s in order:
        new_start = len(out)
        out.extend(tri[s[2]: s[2] + s[3]])
        struct.pack_into('<HH', skin, os_ + s[0] * 48, s[1], new_start >> 16)
        struct.pack_into('<H', skin, os_ + s[0] * 48 + 8, new_start & 0xFFFF)
    for s in subs:
        if s[1] in hide:
            struct.pack_into('<H', skin, os_ + s[0] * 48 + 10, 0)
            struct.pack_into('<HH', skin, os_ + s[0] * 48, s[1], 0)
            struct.pack_into('<H', skin, os_ + s[0] * 48 + 8, 0)
    struct.pack_into('<%dH' % len(out), skin, ot, *out)
    struct.pack_into('<I', skin, 12, len(out))
    return bytes(skin), len(out)


def filter_bank(bank, drop=DROP):
    n = struct.unpack_from('<I', bank, 8)[0]
    keep = []
    for i in range(n):
        rec = bank[12 + i * 156:12 + (i + 1) * 156]
        g, k, t, v, c0, c1, c2 = struct.unpack_from('<7I', rec, 0)
        if k == 0 and (g, t) in drop:
            control, value = c0 & 0xffff, c0 >> 16
            if value != 0:
                continue
        keep.append(rec)
    return bank[:8] + struct.pack('<I', len(keep)) + b''.join(keep)
