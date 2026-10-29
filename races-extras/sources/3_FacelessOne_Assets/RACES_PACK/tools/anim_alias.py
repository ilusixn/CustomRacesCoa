#!/usr/bin/env python3
"""Give creature bodies the player animations they lack, by adding sequences that reuse an existing sequence's animation data.
M2 v264: every per-sequence animation track holds one sub-array per sequence. For each new sequence we append a copy of the
source sequence's M2Sequence entry (new animation id) and, in every per-sequence track in the file (bones, attachments, colors,
texture weights/transforms, events, cameras, particles, ribbons, lights), append the source sequence's sub-array reference
(same data, no copies). Tracks are found by signature (outer arrays of exactly nSeq (count, offset) pairs right after an
interpolation/global-sequence header), so every structure is covered without hard-coding each layout. The sequence lookup
table is rebuilt. Nothing is moved: new tables are appended at the end of the file.
"""
import struct

# player animation ids -> preferred existing sources (first one the model has wins)
STAND, DEATH, WALK, RUN, WOUND, CRIT, ATTACK = 0, 1, 4, 5, 9, 10, 16
CAST = (53, 54, 32, 16, 17)          # spell casts prefer real casts, else ATTACK (the user's ask: attack anims on casts)
WANT = {
    11: (WALK,), 12: (WALK,), 13: (WALK,),                                        # shuffle L/R, walk backwards
    14: (WOUND,), 16: (17, 19, 18), 17: (16, 19, 18), 18: (17, 16, 19), 19: (18, 17, 16), 20: (WOUND, 16),
    21: (WOUND, 16), 22: (WOUND, 16), 23: (WOUND, 16), 24: (WOUND,), 25: (STAND,), 26: (25, STAND), 27: (25, STAND),
    28: (25, STAND), 29: (25, STAND), 30: (WOUND,), 31: (52, 51, STAND), 32: CAST, 33: (54, 53, 32, 16), 34: (STAND,),
    35: (STAND,), 36: (WOUND,), 37: (RUN,), 38: (RUN,), 39: (RUN,), 40: (STAND,), 41: (STAND,), 42: (RUN,), 43: (RUN,),
    44: (RUN,), 45: (WALK,), 46: (16, 17), 47: (16, 17), 48: (25, STAND), 49: (16, 17), 50: (STAND,), 51: (STAND,),
    52: (51, STAND), 53: (54, 32, 16, 17), 54: (53, 32, 16, 17), 55: (16,), 56: (16,), 57: (16,),
    60: (STAND,), 61: (STAND,), 62: (STAND,), 63: (STAND,), 64: (STAND,), 65: (126, 118, 16), 66: (126, 118, STAND),
    67: (126, 118, STAND), 68: (STAND,), 69: (STAND,), 70: (STAND,), 71: (16, 17), 72: (STAND,), 73: (STAND,),
    74: (STAND,), 75: (STAND,), 76: (STAND,), 77: (STAND,), 78: (16, 17), 79: (16, 17), 80: (STAND,), 81: (STAND,),
    91: (STAND,), 95: (STAND,), 96: (STAND,), 97: (STAND,), 98: (STAND,), 99: (STAND,), 100: (STAND,), 101: (STAND,),
    102: (STAND,), 103: (STAND,), 104: (STAND,), 105: (STAND,), 106: (STAND,),
}


def _u32(m, o): return struct.unpack_from("<I", m, o)[0]


def inline_flags(m):
    nseq, oseq = struct.unpack_from("<II", m, 0x1C)
    return [bool(struct.unpack_from("<I", m, oseq + 64 * i + 12)[0] & 0x20) for i in range(nseq)]


def find_tracks(m, nseq):
    """Positions of outer arrays (count == nseq) of per-sequence tracks. Sub-arrays of sequences kept in external .anim
    files (no 0x20 flag) point into those files, so only inline sequences are range-checked."""
    n = len(m); hits = []; inl = inline_flags(m)
    for p in range(0x130, n - 8, 4):
        if _u32(m, p) != nseq:
            continue
        off = _u32(m, p + 4)
        if off < 0x130 or off % 4 or off + 8 * nseq > n:
            continue
        ok = True
        for i in range(nseq):
            c, o = struct.unpack_from("<II", m, off + 8 * i)
            if c > 200000 or (c and inl[i] and (o < 0x130 or o >= n)):
                ok = False; break
        if not ok:
            continue
        interp, gs = struct.unpack_from("<Hh", m, p - 4)            # timestamps outer: header right before
        interp2, gs2 = struct.unpack_from("<Hh", m, p - 12)          # values outer: 8 bytes after timestamps
        if (interp <= 3 and gs >= -1) or (interp2 <= 3 and gs2 >= -1 and _u32(m, p - 8) == nseq):
            hits.append(p)
    return hits


def alias(data, want=WANT, only_missing=True):
    m = bytearray(data)
    nseq, oseq = struct.unpack_from("<II", m, 0x1C)
    seqs = [bytes(m[oseq + 64 * i:oseq + 64 * (i + 1)]) for i in range(nseq)]
    ids = [struct.unpack_from("<H", s)[0] for s in seqs]
    first = {}; inl = inline_flags(m)
    for i, a in enumerate(ids):
        if inl[i]:                      # only inline sequences can be reused (external ones need their own .anim file)
            first.setdefault(a, i)
    have = set(ids)
    add = []                                                         # (new anim id, source seq index)
    for aid, srcs in sorted(want.items()):
        if only_missing and aid in have:
            continue
        src = next((first[s] for s in srcs if s in first), None)
        if src is not None:
            add.append((aid, src))
    if not add:
        return bytes(m), []
    tracks = find_tracks(m, nseq)
    def put(blob):
        while len(m) % 16:
            m.append(0)
        o = len(m); m.extend(blob); return o
    for p in tracks:                                                  # extend each outer array: src sub-array refs
        off = _u32(m, p + 4)
        pairs = [m[off + 8 * i:off + 8 * i + 8] for i in range(nseq)]
        pairs += [pairs[src] for _, src in add]
        struct.pack_into("<II", m, p, nseq + len(add), put(b"".join(pairs)))
    new = []
    for k, (aid, src) in enumerate(add):
        s = bytearray(seqs[src]); idx = nseq + k
        struct.pack_into("<HH", s, 0, aid, 0)                         # id, variation 0
        struct.pack_into("<I", s, 12, struct.unpack_from("<I", s, 12)[0] & ~0x40)   # not an alias: data refs copied
        struct.pack_into("<hH", s, 60, -1, idx)                        # variationNext none, aliasNext self
        new.append(bytes(s))
    struct.pack_into("<II", m, 0x1C, nseq + len(add), put(b"".join(seqs + new)))
    allids = ids + [a for a, _ in add]
    look = [-1] * (max(allids) + 1)
    for i, a in enumerate(allids):
        if look[a] == -1:
            look[a] = i
    struct.pack_into("<II", m, 0x24, len(look), put(struct.pack("<%dh" % len(look), *look)))
    return bytes(m), add


def add_attach(data, pairs):
    """pairs: (new id, copy-from id). Adds player attachment points (weapons 1/2, shield 0, back 12) that a creature body lacks,
    cloned from an existing point (same bone/position/visibility track). Rebuilds the attachment lookup."""
    m = bytearray(data)
    na, oa = struct.unpack_from("<II", m, 0xF0)
    ents = [bytes(m[oa + 40 * i:oa + 40 * (i + 1)]) for i in range(na)]
    have = {struct.unpack_from("<I", e)[0]: e for e in ents}; added = []
    for nid, src in pairs:
        if nid not in have and src in have:
            e = bytearray(have[src]); struct.pack_into("<I", e, 0, nid); ents.append(bytes(e)); have[nid] = bytes(e); added.append(nid)
    if not added:
        return bytes(m), []
    def put(blob):
        while len(m) % 16:
            m.append(0)
        o = len(m); m.extend(blob); return o
    struct.pack_into("<II", m, 0xF0, len(ents), put(b"".join(ents)))
    ids = [struct.unpack_from("<I", e)[0] for e in ents]; look = [-1] * (max(ids) + 1)
    for i, a in enumerate(ids):
        if look[a] == -1:
            look[a] = i
    struct.pack_into("<II", m, 0xF8, len(look), put(struct.pack("<%dh" % len(look), *look)))
    return bytes(m), added


# right hand 1 <- spell right 22, left hand 2 / shield 0 <- spell left 21, back 12 <- chest-back 16
PLAYER_ATTACH = [(1, 22), (2, 21), (0, 21), (12, 16)]


def playerize(data):
    """attachments then animation aliases; returns (bytes, report)"""
    d, att = add_attach(data, PLAYER_ATTACH)
    d, add = alias(d)
    return d, f"attach +{att}, anims +{len(add)}"


# display scales: model top (bind pose, feet at 0) brought to about human height (human male 2.1, female 1.93; bodies a bit
# heroic: male ~2.3, female ~2.1)
def fscale(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def height(data):
    lo = struct.unpack_from("<3f", data, 0xA0); hi = struct.unpack_from("<3f", data, 0xAC)
    return hi[2] - lo[2]


if __name__ == "__main__":
    import sys
    d = open(sys.argv[1], "rb").read(); out, add = alias(d)
    print(len(add), "added:", add[:12], "... tracks", len(find_tracks(d, struct.unpack_from("<I", d, 0x1C)[0])), "height", round(height(d), 3))
    if len(sys.argv) > 2:
        open(sys.argv[2], "wb").write(out)
