"""Restore a pinned Windows runtime source variant without Git or network access."""
from pathlib import Path, PurePosixPath
import argparse, hashlib, json, os, zipfile

def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest().upper()

def restore(variant, destination):
    root = Path(__file__).resolve().parent
    mapping = json.loads((root / 'SOURCE-DELIVERY-MANIFEST.json').read_text())
    choice = mapping['variants'][variant]
    destination = Path(destination).absolute()
    if destination.exists():
        raise ValueError('Choose a new, empty destination; existing source is never overwritten.')
    if os.name == 'nt':
        destination = Path('\\\\?\\' + str(destination))
    destination.mkdir(parents=True)
    restored = {}
    for relative, expected in [(choice['archive'], choice['sha256']),
                               (mapping['dependencies']['archive'], mapping['dependencies']['sha256'])]:
        archive = root / relative
        if digest(archive) != expected:
            raise ValueError('Source archive hash mismatch: ' + relative)
        with zipfile.ZipFile(archive) as z:
            for item in z.infolist():
                path = PurePosixPath(item.filename)
                if path.is_absolute() or '..' in path.parts or ':' in item.filename or '\\' in item.filename:
                    raise ValueError('Unsafe archive path: ' + item.filename)
                if item.is_dir():
                    continue
                if item.filename in restored:
                    raise ValueError('Overlapping source inputs: ' + item.filename)
                data = z.read(item)
                output = destination.joinpath(*path.parts)
                output.parent.mkdir(parents=True, exist_ok=True)
                output.write_bytes(data)
                restored[item.filename] = hashlib.sha256(data).hexdigest().upper()
    receipt = {'variant': variant, 'commit': choice['commit'], 'files': restored,
               'scope': 'Pinned source plus verified dependency archive; no compiler, network or runtime execution',
               'licensingStatus': 'REVIEW REQUIRED; not a distribution clearance'}
    (destination / 'SOURCE-RESTORE-RECEIPT.json').write_text(json.dumps(receipt, indent=2))
    print(f'Restored {len(restored)} verified files for {variant}.')
    return receipt

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--variant', choices=['Solo-Local', 'LAN'], required=True)
    parser.add_argument('--destination', required=True)
    args = parser.parse_args()
    restore(args.variant, args.destination)
