#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify the v17.99.6 package built using the correct FM-1 cross-toolchain.
Usage: python3 tests/validate_built_fwsc.py app/build/amb-17.99.6.fwsc
"""
from pathlib import Path
import hashlib
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'base/tools'))
import fm1pkg_make as pkg
fw=Path(sys.argv[1] if len(sys.argv)>1 else ROOT/'app/build/amb-17.99.6.fwsc')
app=ROOT/'app/build/amb.bin'
ldr=ROOT/'app/build/loader/ota.bin'
if not fw.is_file() or not app.is_file() or not ldr.is_file():
    raise SystemExit('Missing target artifact; build with the JieLi toolchain and SDK first.')
r=fw.read_bytes()
assert len(r)==610108,len(r)
id=''.join(chr((r[i*48+47]-i-1)&255) for i in range(20) if r[i*48+47]!=0x7d)
assert id=='FM-1_7179906',id
p=b''.join(r[i*48:i*48+47] for i in range(20))+r[960:]
h=bytearray(p[:64]);pkg.enc(h,0,64)
assert pkg.crc16(h[2:])==struct.unpack_from('<H',h)[0]
size,count=struct.unpack_from('<IH',h,4)
assert size==len(p)
assert pkg.crc16(p[64:64+count*80])==struct.unpack_from('<H',h,2)[0]
files={}
for i in range(count):
    entry=bytearray(p[64+i*80:144+i*80]);pkg.enc(entry,0,80)
    offset,n=struct.unpack_from('<II',entry,8)
    payload=p[offset:offset+n]
    assert len(payload)==n and pkg.crc16(payload)==struct.unpack_from('<H',entry,4)[0]
    files[entry[64:].split(b'\x00')[0].decode()]=payload
assert files['ota.bin']==ldr.read_bytes()
flash=bytearray(files['flash.bin'][0x4000:]);pkg.sfc(flash,0,len(flash),0,pkg.KEY)
binary=app.read_bytes()
assert flash[0x120:0x120+len(binary)]==binary
assert flash[0x120+len(binary):0x120+pkg.APP_SLOT]==b'\xff'*(pkg.APP_SLOT-len(binary))
print(f'PASS {id}: CRCs, loader, decrypted app, padding, length {len(r)}, app {len(binary)} bytes')
print('SHA256',hashlib.sha256(r).hexdigest())
