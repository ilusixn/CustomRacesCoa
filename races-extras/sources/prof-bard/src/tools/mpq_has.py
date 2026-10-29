"""Dit quels fichiers existent dans les archives MPQ d'un client WoW 3.3.5 (sans tout extraire).

Usage : python mpq_has.py <dossier Data> <fichier liste.txt>
La liste contient un chemin par ligne (séparateurs « \\ »). Le script lit seulement la table de hachage
de chaque archive et affiche, pour chaque chemin, les archives qui le contiennent.
"""

import struct
import sys
from pathlib import Path


def crypt_table():
    table, seed = [0] * 0x500, 0x00100001
    for i in range(0x100):
        index = i
        for _ in range(5):
            seed = (seed * 125 + 3) % 0x2AAAAB
            high = (seed & 0xFFFF) << 16
            seed = (seed * 125 + 3) % 0x2AAAAB
            table[index] = high | (seed & 0xFFFF)
            index += 0x100
    return table


CRYPT = crypt_table()


def hash_string(text, kind):
    seed1, seed2 = 0x7FED7FED, 0xEEEEEEEE
    for char in text.upper().encode("ascii"):
        seed1 = CRYPT[kind * 0x100 + char] ^ ((seed1 + seed2) & 0xFFFFFFFF)
        seed2 = (char + seed1 + seed2 + ((seed2 << 5) & 0xFFFFFFFF) + 3) & 0xFFFFFFFF
    return seed1


def decrypt(data, key):
    values = list(struct.unpack(f"<{len(data) // 4}I", data))
    seed1, seed2 = key, 0xEEEEEEEE
    for i, value in enumerate(values):
        seed2 = (seed2 + CRYPT[0x400 + (seed1 & 0xFF)]) & 0xFFFFFFFF
        value ^= (seed1 + seed2) & 0xFFFFFFFF
        seed1 = (((~seed1 << 0x15) + 0x11111111) | (seed1 >> 0x0B)) & 0xFFFFFFFF
        seed2 = (value + seed2 + (seed2 << 5) + 3) & 0xFFFFFFFF
        values[i] = value
    return values


def read_hash_table(path):
    with path.open("rb") as f:
        head = f.read(4)
        base = 0
        if head == b"MPQ\x1b":                      # en-tête utilisateur : l'archive commence plus loin
            base = struct.unpack("<I", f.read(8)[4:8])[0]
        f.seek(base)
        if f.read(4) != b"MPQ\x1a":
            return None
        header = f.read(0x28)
        _size, _archive, version, _shift, hash_off, _block_off, hash_count, _block_count = struct.unpack("<IIHHIIII", header[:28])
        if version >= 1:                            # v2 : 16 bits de poids fort du décalage, à 0x28
            f.seek(base + 0x28)
            hash_off |= struct.unpack("<H", f.read(2))[0] << 32
        f.seek(base + hash_off)
        raw = f.read(hash_count * 16)
    values = decrypt(raw, hash_string("(hash table)", 3))
    return hash_count, values


def has(table, name):
    count, values = table
    start = hash_string(name, 0) & (count - 1)
    a, b = hash_string(name, 1), hash_string(name, 2)
    i = start
    while True:
        e = values[i * 4:i * 4 + 4]
        if e[3] == 0xFFFFFFFF:
            return False
        if e[0] == a and e[1] == b and e[3] < 0xFFFFFFFE:
            return True
        i = (i + 1) & (count - 1)
        if i == start:
            return False


def main():
    data, listing = Path(sys.argv[1]), Path(sys.argv[2])
    names = [line.strip() for line in listing.read_text(encoding="utf-8").splitlines() if line.strip()]
    tables = {}
    for mpq in sorted(data.rglob("*.[Mm][Pp][Qq]")):
        try:
            table = read_hash_table(mpq)
        except Exception:
            table = None
        if table and table[0] & (table[0] - 1) == 0:
            tables[mpq.name] = table
    print(f"{len(tables)} archives lues", file=sys.stderr)
    for name in names:
        found = [n for n, t in tables.items() if has(t, name)]
        print(("OUI " if found else "non ") + name + ("   [" + ", ".join(found[-2:]) + "]" if found else ""))


if __name__ == "__main__":
    main()
