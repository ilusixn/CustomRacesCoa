"""Verify and unpack the race assets into a new, empty staging directory."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zlib


def safe(root, rel):
    p = PurePosixPath(rel)
    if p.is_absolute() or '..' in p.parts or ':' in str(p) or '\\' in str(p):
        raise ValueError('Unsafe manifest path')
    result = root / Path(p)
    if not result.resolve().is_relative_to(root.resolve()):
        raise ValueError('Path escapes staging root')
    return result


def unpack(package, output):
    if output.exists() and any(output.iterdir()):
        raise ValueError('Use a new empty output directory, not a live client')
    spec = json.loads((package / 'assets-manifest.json').read_text(encoding='utf-8'))
    output.mkdir(parents=True, exist_ok=True)
    for asset in spec['assets']:
        dest = safe(output, asset['path'])
        dest.parent.mkdir(parents=True, exist_ok=True)
        decoder = zlib.decompressobj()
        total = 0
        digest = hashlib.sha256()
        with dest.open('xb') as f:
            for part in asset['parts']:
                p = safe(package, part['path'])
                part_hash = hashlib.sha256(); part_bytes = 0
                with p.open('rb') as source:
                    while b := source.read(1024 * 1024):
                        part_hash.update(b); part_bytes += len(b)
                        raw = decoder.decompress(b)
                        total += len(raw)
                        if total > asset['bytes']:
                            raise ValueError('Unexpected expanded asset size')
                        digest.update(raw); f.write(raw)
                if part_bytes != part['bytes'] or part_hash.hexdigest() != part['sha256']:
                    raise ValueError('Compressed asset part failed verification')
            raw = decoder.flush(); digest.update(raw); f.write(raw); total += len(raw)
        if not decoder.eof or total != asset['bytes'] or digest.hexdigest() != asset['sha256']:
            raise ValueError('Unpacked asset failed verification')
    return {'verifiedAssets': len(spec['assets']), 'output': str(output)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(unpack(args.package, args.output), indent=2))
