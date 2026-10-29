"""Extrait un fichier d'une archive MPQ du client WoW 3.3.5 (formats zlib et bzip2, sans chiffrement).

Usage : python mpq_extract.py <archive.MPQ> <chemin dans l'archive> <fichier de sortie>
Sert à lire les fichiers d'interface (.xml, .lua) que le client remplace, pour en reprendre la disposition.
"""

import bz2
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mpq_has import decrypt, hash_string

FLAG_COMPRESS, FLAG_ENCRYPTED, FLAG_SINGLE_UNIT = 0x200, 0x10000, 0x01000000


def decompress(chunk):
    mask, data = chunk[0], chunk[1:]
    if mask & 0x08 or mask & 0x01 or mask & 0x40 or mask & 0x80:
        raise SystemExit(f"compression non prise en charge (masque 0x{mask:02x})")
    if mask & 0x10:
        data = bz2.decompress(data)
    if mask & 0x02:
        data = zlib.decompress(data)
    return data


def extract(archive, name):
    with Path(archive).open("rb") as f:
        head = f.read(4)
        base = 0
        if head == b"MPQ\x1b":
            base = struct.unpack("<I", f.read(8)[4:8])[0]
        f.seek(base)
        assert f.read(4) == b"MPQ\x1a"
        header = f.read(0x28)
        _size, _total, version, shift, hash_off, block_off, hash_count, block_count = struct.unpack("<IIHHIIII", header[:28])
        sector_size = 512 << shift
        f.seek(base + hash_off)
        hashes = decrypt(f.read(hash_count * 16), hash_string("(hash table)", 3))
        f.seek(base + block_off)
        blocks = decrypt(f.read(block_count * 16), hash_string("(block table)", 3))

        start = hash_string(name, 0) & (hash_count - 1)
        a, b = hash_string(name, 1), hash_string(name, 2)
        i, block = start, None
        while True:
            e = hashes[i * 4:i * 4 + 4]
            if e[3] == 0xFFFFFFFF:
                break
            if e[0] == a and e[1] == b and e[3] < 0xFFFFFFFE:
                block = e[3]
                break
            i = (i + 1) & (hash_count - 1)
            if i == start:
                break
        if block is None:
            raise SystemExit(f"{name} : absent de {Path(archive).name}")

        position, compressed, size, flags = blocks[block * 4:block * 4 + 4]
        if flags & FLAG_ENCRYPTED:
            raise SystemExit("fichier chiffré : non pris en charge")
        f.seek(base + position)
        raw = f.read(compressed)

    if flags & FLAG_SINGLE_UNIT:
        return decompress(raw) if flags & FLAG_COMPRESS and compressed < size else raw
    if not flags & FLAG_COMPRESS:
        return raw[:size]
    sectors = (size + sector_size - 1) // sector_size
    offsets = struct.unpack(f"<{sectors + 1}I", raw[:(sectors + 1) * 4])
    out = bytearray()
    for k in range(sectors):
        chunk = raw[offsets[k]:offsets[k + 1]]
        expected = min(sector_size, size - k * sector_size)
        out += decompress(chunk) if len(chunk) < expected else chunk
    return bytes(out)


if __name__ == "__main__":
    data = extract(sys.argv[1], sys.argv[2])
    Path(sys.argv[3]).write_bytes(data)
    print(f"{len(data)} octets -> {sys.argv[3]}")
