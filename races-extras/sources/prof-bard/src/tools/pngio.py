"""Lecture de PNG 8 bits RGBA/RGB sans bibliothèque, et écriture de TGA 32 bits (format des textures du client WoW 3.3.5)."""

import struct
import zlib


def read_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "pas un PNG"
    pos, idat, header = 8, [], None
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat.append(body)
        pos += 12 + length
    width, height, depth, ctype, _, _, interlace = header
    assert depth == 8 and interlace == 0 and ctype in (2, 6), "seuls les PNG 8 bits RGB/RGBA non entrelacés sont gérés"
    bpp = 4 if ctype == 6 else 3
    raw = zlib.decompress(b"".join(idat))
    stride = width * bpp
    pixels = bytearray(width * height * 4)
    prev = bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        ftype = raw[start]
        line = bytearray(raw[start + 1:start + 1 + stride])
        if ftype == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 255
        elif ftype == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif ftype == 3:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 255
        elif ftype == 4:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 255
        prev = line
        row = y * width * 4
        if bpp == 4:
            pixels[row:row + stride] = line
        else:
            for x in range(width):
                pixels[row + x * 4:row + x * 4 + 3] = line[x * 3:x * 3 + 3]
                pixels[row + x * 4 + 3] = 255
    return width, height, pixels          # pixels : RGBA, ligne par ligne, du haut vers le bas


def write_tga(path, width, height, pixels):
    """TGA 32 bits non compressé, origine en haut à gauche, pixels en RGBA."""
    out = bytearray(struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, width, height, 32, 8 | 0x20))
    bgra = bytearray(len(pixels))
    bgra[0::4], bgra[1::4], bgra[2::4], bgra[3::4] = pixels[2::4], pixels[1::4], pixels[0::4], pixels[3::4]
    open(path, "wb").write(bytes(out) + bytes(bgra))
