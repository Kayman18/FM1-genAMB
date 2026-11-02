#!/usr/bin/env python3
"""Validate the installable v17.99 package against this source tree's build."""
from pathlib import Path
import hashlib
import struct
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'base/tools'))
import fm1pkg_make as package

fw = root / 'app/build/amb-17.99.fwsc'
raw = fw.read_bytes()
packed = b''.join(raw[i*48:i*48+47] for i in range(20)) + raw[960:]
identity = ''.join(chr((raw[i*48+47]-i-1) & 255)
                   for i in range(20) if raw[i*48+47] != 0x7d)
assert identity == 'FM-1_7179900', identity
header = bytearray(packed[:64])
package.enc(header, 0, 64)
assert package.crc16(header[2:]) == struct.unpack_from('<H', header)[0]
total, count = struct.unpack_from('<IH', header, 4)
assert total == len(packed)
assert package.crc16(packed[64:64+count*80]) == struct.unpack_from('<H', header, 2)[0]
parts = {}
for i in range(count):
    entry = bytearray(packed[64+i*80:144+i*80])
    package.enc(entry, 0, 80)
    offset, size = struct.unpack_from('<II', entry, 8)
    data = packed[offset:offset+size]
    assert len(data) == size
    assert package.crc16(data) == struct.unpack_from('<H', entry, 4)[0]
    parts[entry[64:].split(b'\0')[0].decode()] = data
assert parts['ota.bin'] == (root/'app/build/loader/ota.bin').read_bytes()
flash = bytearray(parts['flash.bin'][0x4000:])
package.sfc(flash, 0, len(flash), 0, package.KEY)
app = (root/'app/build/amb.bin').read_bytes()
assert flash[0x120:0x120+len(app)] == app
assert flash[0x120+len(app):0x120+package.APP_SLOT] == b'\xff'*(package.APP_SLOT-len(app))
print('PASS: identity, header/table/payload CRCs, loader, decrypted app and slot padding.')
print(f'Identity {identity}; firmware {len(raw)} bytes; application {len(app)} bytes.')
print('SHA256', hashlib.sha256(raw).hexdigest())
