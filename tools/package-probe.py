"""Create a single-file KoboRoot package for a compiled diagnostic plugin.

This does not install anything. Inspect and back up the device before staging.
"""
import argparse
import hashlib
import io
from pathlib import Path
import struct
import tarfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('library', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--component', choices=['probe', 'links'], default='probe')
args = parser.parse_args()
data = args.library.read_bytes()
if len(data) < 52 or data[:6] != b'\x7fELF\x01\x01' or struct.unpack_from('<H', data, 18)[0] != 40:
    raise SystemExit('Expected an ELF32 little-endian ARM library')
if struct.unpack_from('<H', data, 16)[0] != 3:
    raise SystemExit('Expected an ELF shared object')
with args.output.open('xb') as output:
    with tarfile.open(fileobj=output, mode='w:gz', format=tarfile.USTAR_FORMAT) as archive:
        name = 'libkobolinkprobe.so' if args.component == 'probe' else 'libkobobooklinks.so'
        entry = tarfile.TarInfo('./usr/local/Kobo/imageformats/' + name)
        entry.size = len(data)
        entry.mode = 0o644
        entry.uid = entry.gid = 0
        entry.uname = entry.gname = 'root'
        entry.mtime = 0
        archive.addfile(entry, io.BytesIO(data))
print('Library SHA-256:', hashlib.sha256(data).hexdigest())
print('Package SHA-256:', hashlib.sha256(args.output.read_bytes()).hexdigest())
