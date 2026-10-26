"""Compare two TGA screenshots pixel by pixel.

Usage: python Tools/coa/imgdiff.py A.tga B.tga [--out diff.tga]
Prints `identical` (exit 0) or `N pixels differ (max channel delta D)` (exit 1); different sizes exit 2.
Reads uncompressed and RLE TGA, 24/32-bit, either origin (top-left or bottom-left); alpha is ignored.
--out writes an uncompressed 24-bit TGA: differing pixels red, identical ones the darkened source.
Used for the render pipeline's pixel-identical proof (see CoA/DIFFERENCES.md).
"""
import struct
import sys


def read_tga(path):
    """Return (width, height, rows) with rows top-to-bottom, each row a bytes of width*3 BGR."""
    data = open(path, 'rb').read()
    idlen, cmap, itype, _, _, _, _, _, width, height, bpp, desc = struct.unpack_from('<BBBHHBHHHHBB', data, 0)
    if cmap or itype not in (2, 10) or bpp not in (24, 32):
        raise ValueError(f'{path}: unsupported TGA (colour-map {cmap}, type {itype}, {bpp} bpp)')
    bytes_pp = bpp // 8
    pos = 18 + idlen
    total = width * height * bytes_pp
    if itype == 2:
        raw = data[pos:pos + total]
    else:
        out = bytearray()
        while len(out) < total:
            head = data[pos]
            pos += 1
            count = (head & 0x7F) + 1
            if head & 0x80:
                out += data[pos:pos + bytes_pp] * count
                pos += bytes_pp
            else:
                out += data[pos:pos + count * bytes_pp]
                pos += count * bytes_pp
        raw = bytes(out[:total])
    if len(raw) != total:
        raise ValueError(f'{path}: truncated pixel data')
    stride = width * bytes_pp
    rows = []
    for y in range(height):
        row = raw[y * stride:(y + 1) * stride]
        if bytes_pp == 4:
            bgr = bytearray(width * 3)
            bgr[0::3], bgr[1::3], bgr[2::3] = row[0::4], row[1::4], row[2::4]
            row = bytes(bgr)
        rows.append(row)
    if not desc & 0x20:  # bit 5 clear: stored bottom-up
        rows.reverse()
    return width, height, rows


def write_tga(path, width, height, rows):
    with open(path, 'wb') as f:
        f.write(struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, width, height, 24, 0x20))
        f.write(b''.join(rows))


def diff(rows_a, rows_b, width):
    count = 0
    maxdelta = 0
    out = []
    for ra, rb in zip(rows_a, rows_b):
        if ra == rb:
            out.append(bytes(c >> 2 for c in ra))
            continue
        row = bytearray(c >> 2 for c in ra)
        for x in range(width):
            i = x * 3
            if ra[i:i + 3] != rb[i:i + 3]:
                count += 1
                maxdelta = max(maxdelta, *(abs(ra[i + k] - rb[i + k]) for k in range(3)))
                row[i:i + 3] = b'\x00\x00\xff'
        out.append(bytes(row))
    return count, maxdelta, out


def main(argv):
    args = argv[1:]
    out_path = None
    if '--out' in args:
        i = args.index('--out')
        out_path = args[i + 1]
        del args[i:i + 2]
    if len(args) != 2:
        print(__doc__)
        return 2
    wa, ha, rows_a = read_tga(args[0])
    wb, hb, rows_b = read_tga(args[1])
    if (wa, ha) != (wb, hb):
        print(f'size differs: {wa}x{ha} vs {wb}x{hb}')
        return 2
    count, maxdelta, out = diff(rows_a, rows_b, wa)
    if out_path:
        write_tga(out_path, wa, ha, out)
    if count == 0:
        print('identical')
        return 0
    print(f'{count} pixels differ (max channel delta {maxdelta})')
    return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
