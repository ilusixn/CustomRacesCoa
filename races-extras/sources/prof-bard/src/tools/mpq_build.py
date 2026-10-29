"""Construit un MPQ minimal (fichiers non compressés), repris du build du patch Naga."""
import struct
import sys
from pathlib import Path

HASH_OFFSET, HASH_NAME_A, HASH_NAME_B, HASH_TABLE = 0, 1, 2, 3
SECTOR_SHIFT = 3
FLAGS_FILE = 0x80000000 | 0x01000000


def build_crypt_table():
    table = [0] * 0x500
    seed = 0x00100001
    for i in range(0x100):
        index = i
        for _ in range(5):
            seed = (seed * 125 + 3) % 0x2AAAAB
            high = (seed & 0xFFFF) << 16
            seed = (seed * 125 + 3) % 0x2AAAAB
            table[index] = high | (seed & 0xFFFF)
            index += 0x100
    return table


CRYPT = build_crypt_table()


def hash_string(text, kind):
    seed1, seed2 = 0x7FED7FED, 0xEEEEEEEE
    for char in text.upper().encode("ascii"):
        seed1 = CRYPT[kind * 0x100 + char] ^ ((seed1 + seed2) & 0xFFFFFFFF)
        seed2 = (char + seed1 + seed2 + ((seed2 << 5) & 0xFFFFFFFF) + 3) & 0xFFFFFFFF
    return seed1


def encrypt(values, key):
    seed1, seed2 = key, 0xEEEEEEEE
    out = []
    for value in values:
        seed2 = (seed2 + CRYPT[0x400 + (seed1 & 0xFF)]) & 0xFFFFFFFF
        out.append(value ^ ((seed1 + seed2) & 0xFFFFFFFF))
        seed1 = ((((~seed1) << 0x15) + 0x11111111) | (seed1 >> 0x0B)) & 0xFFFFFFFF
        seed2 = (value + seed2 + ((seed2 << 5) & 0xFFFFFFFF) + 3) & 0xFFFFFFFF
    return out


def build(files):
    table_size = 16
    while table_size < len(files) * 2:
        table_size *= 2
    header_size = 32
    data = b""
    blocks = []
    for _, content in files:
        blocks.append((header_size + len(data), len(content), len(content), FLAGS_FILE))
        data += content
    hashes = [(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF, 0xFFFF, 0xFFFFFFFF)] * table_size
    for index, (name, _) in enumerate(files):
        slot = hash_string(name, HASH_OFFSET) & (table_size - 1)
        while hashes[slot][4] != 0xFFFFFFFF:
            slot = (slot + 1) & (table_size - 1)
        hashes[slot] = (hash_string(name, HASH_NAME_A), hash_string(name, HASH_NAME_B), 0, 0, index)
    hash_words = [w for entry in hashes for w in (entry[0], entry[1], entry[2] | (entry[3] << 16), entry[4])]
    block_words = [w for block in blocks for w in block]
    hash_bytes = struct.pack(f"<{len(hash_words)}I", *encrypt(hash_words, hash_string("(hash table)", HASH_TABLE)))
    block_bytes = struct.pack(f"<{len(block_words)}I", *encrypt(block_words, hash_string("(block table)", HASH_TABLE)))
    hash_offset = header_size + len(data)
    block_offset = hash_offset + len(hash_bytes)
    archive_size = block_offset + len(block_bytes)
    header = struct.pack("<4sIIHHIIII", b"MPQ\x1a", header_size, archive_size, 0, SECTOR_SHIFT, hash_offset,
                         block_offset, table_size, len(blocks))
    assert len(header) == header_size
    return header + data + hash_bytes + block_bytes


def main(source, target, prefix):
    files = [(f"{prefix}\\{path.name}", path.read_bytes()) for path in sorted(Path(source).glob("*.dbc"))]
    Path(target).write_bytes(build(files))
    print(len(files), "fichiers ->", target)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
