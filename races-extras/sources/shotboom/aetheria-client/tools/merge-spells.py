"""Merge only the package-owned WotLK Spell.dbc rows into a new output file."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def merge(input_path, output_path, data_path, replace_owned=False):
    if output_path.exists() or input_path.resolve() == output_path.resolve():
        raise ValueError('Choose a new output file; the input is never overwritten')
    spec = json.loads(data_path.read_text(encoding='utf-8'))
    data = input_path.read_bytes()
    magic, count, fields, size, pool_size = struct.unpack_from('<4s4I', data)
    if (magic, fields, size) != (b'WDBC', spec['fields'], spec['recordSize']):
        raise ValueError('Unsupported Spell.dbc schema')
    if len(data) != 20 + count * size + pool_size:
        raise ValueError('Invalid Spell.dbc size')
    rows = [data[20 + i * size:20 + (i + 1) * size] for i in range(count)]
    by_id = {struct.unpack_from('<I', row)[0]: row for row in rows}
    if len(by_id) != count:
        raise ValueError('Duplicate input spell IDs')
    pool = bytearray(data[20 + count * size:])
    old_pool_size = len(pool)
    replacements = {}
    known = {}
    for row in spec['rows']:
        identity = row['id']
        if identity in by_id:
            if not replace_owned:
                raise ValueError('Owned ID already exists; review ownership before --replace-owned')
            # Permit an update only when the existing spell has the expected owned name.
            name_index = spec['englishNameField']
            old_offset = struct.unpack_from('<I', by_id[identity], name_index * 4)[0]
            if old_offset >= old_pool_size:
                raise ValueError('Invalid existing owned string offset')
            old_name = bytes(pool[old_offset:pool.index(0, old_offset)]).decode('utf-8')
            if old_name != row['strings'][str(name_index)]:
                raise ValueError('Spell ID belongs to a differently named spell')
        words = list(row['words'])
        if len(words) != fields or words[0] != identity or identity not in spec['ownedIds']:
            raise ValueError('Invalid package row')
        for column, value in row['strings'].items():
            encoded = value.encode('utf-8') + b'\0'
            if encoded not in known:
                known[encoded] = len(pool)
                pool.extend(encoded)
            words[int(column)] = known[encoded]
        replacements[identity] = struct.pack('<' + str(fields) + 'I', *words)
    if set(replacements) != set(spec['ownedIds']):
        raise ValueError('Missing or unexpected package spell IDs')
    result = [replacements.get(struct.unpack_from('<I', row)[0], row) for row in rows]
    result.extend(replacements[i] for i in sorted(set(replacements) - set(by_id)))
    for before, after in zip(rows, result):
        if struct.unpack_from('<I', before)[0] not in replacements and before != after:
            raise ValueError('Unrelated row changed')
    if pool[:old_pool_size] != data[20 + count * size:]:
        raise ValueError('Original string pool changed')
    output = struct.pack('<4s4I', magic, len(result), fields, size, len(pool)) + b''.join(result) + pool
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open('xb') as f:
        f.write(output)
    return {'ownedRows': len(replacements), 'unrelatedRowsPreserved': count - len(set(by_id) & set(replacements)),
            'originalStringPoolPreserved': True, 'outputSha256': hashlib.sha256(output).hexdigest()}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--data', type=Path, default=Path(__file__).resolve().parents[1] / 'data/spells.json')
    parser.add_argument('--replace-owned', action='store_true')
    args = parser.parse_args()
    print(json.dumps(merge(args.input, args.output, args.data, args.replace_owned), indent=2))
