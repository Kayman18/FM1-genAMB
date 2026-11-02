#!/usr/bin/env python3
from pathlib import Path
import sys,struct,hashlib
root=Path('fix-restore').resolve()
sys.path.insert(0,str(root/'base/tools'));import fm1pkg_make as m
raw=(root/'app/build/amb-17.22.fwsc').read_bytes()
f=b''.join(raw[i*48:i*48+47] for i in range(20))+raw[960:]
identity=''.join(chr((raw[i*48+47]-i-1)&255) for i in range(20) if raw[i*48+47]!=0x7d)
assert identity=='FM-1_7172200'
h=bytearray(f[:64]);m.enc(h,0,64)
assert m.crc16(h[2:])==struct.unpack_from('<H',h)[0]
total,n=struct.unpack_from('<IH',h,4);assert total==len(f)
assert m.crc16(f[64:64+n*80])==struct.unpack_from('<H',h,2)[0]
payload={}
for i in range(n):
 e=bytearray(f[64+i*80:144+i*80]);m.enc(e,0,80);off,size=struct.unpack_from('<II',e,8);data=f[off:off+size]
 assert len(data)==size and m.crc16(data)==struct.unpack_from('<H',e,4)[0]
 payload[e[64:].split(b'\0')[0].decode()]=data
assert payload['ota.bin']==(root/'app/build/loader/ota.bin').read_bytes()
r=bytearray(payload['flash.bin'][0x4000:]);m.sfc(r,0,len(r),0,m.KEY)
app=(root/'app/build/amb.bin').read_bytes();assert r[0x120:0x120+len(app)]==app
assert r[0x120+len(app):0x120+m.APP_SLOT]==b'\xff'*(m.APP_SLOT-len(app))
print('PASS: identity, total length, header/table/payload CRCs, loader, decrypted app bytes and slot padding.')
print('identity:',identity,'package bytes:',len(raw),'app bytes:',len(app))
print('SHA256:',hashlib.sha256(raw).hexdigest())
