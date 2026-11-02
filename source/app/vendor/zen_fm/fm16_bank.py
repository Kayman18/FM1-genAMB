#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Recreate the exact 16 built-in FM presets from the released v17.99.2 bank.

The asset ``fm16_expanded_bank.bin`` is the canonical byte-exact expanded
156-byte voice bank recovered from the user's v17.99.2 firmware image. It is
included with the source so subsequent builds do not depend on the old binary.

Usage: python3 fm16_bank.py expanded OUTPUT.h
       python3 fm16_bank.py packed OUTPUT.h
"""
from pathlib import Path
import sys
import hashlib

BANK_SHA256 = '3b3bf7b1eeedf65b93038533a39b3eb5a7e12f51b7e674df8d061bdfd5b118d1'  # replaced by inventory script after extraction
N = 16
NAME_MAP = [
    'KOTO','HARP','BRT_HARP','DROP','MALLET','BELL','RIN','BAMBOO',
    'FURIN','MARIMBA','CELESTE','KALIMBA','XYLOPHONE','GLOCKEN','WARM_BASS','RESO_BASS'
]


def load():
    bank = (Path(__file__).parent / 'fm16_expanded_bank.bin').read_bytes()
    if len(bank) != 16 * 156:
        raise ValueError(f'Expected 2496 bytes, received {len(bank)}')
    digest = hashlib.sha256(bank).hexdigest()
    if digest != BANK_SHA256:
        raise ValueError(f'Preset bank SHA256 mismatch: {digest}')
    return [bank[i*156:(i+1)*156] for i in range(N)]


def pack(v):
    b = bytearray(128)
    for k in range(6):
        o=v[k*21:(k+1)*21]
        d=k*17
        b[d:d+11]=o[:11]
        b[d+11]=(o[11] & 3) | ((o[12]&3)<<2)
        b[d+12]=(o[13] & 7) | ((o[20]&15)<<3)
        b[d+13]=(o[14] & 3) | ((o[15]&7)<<2)
        b[d+14]=o[16] & 0x7F
        b[d+15]=(o[17]&1) | ((o[18]&31)<<1)
        b[d+16]=o[19] & 0x7F
    b[102:111]=v[126:135]
    b[110]&=31
    b[111]=(v[135]&7) | ((v[136]&1)<<3)
    b[112:116]=v[137:141]
    b[116]=(v[141]&1) | ((v[142]&7)<<1) | ((v[143]&7)<<4)
    b[117]=v[144] & 0x7F
    b[118:128]=v[145:155]
    return bytes(b)


def render(fmt):
    bank=load()
    if fmt == 'expanded':
        output=['/* SPDX-License-Identifier: GPL-3.0-only */',
                '/* 16 voice presets recovered byte-exact from released genAMB v17.99.2. */',
                '#pragma once','#include <stdint.h>',
                '#define FM_FACTORY_COUNT 16',
                'static const uint8_t FM_FACTORY[FM_FACTORY_COUNT][156] = {']
        data=bank
    elif fmt == 'packed':
        output=['/* SPDX-License-Identifier: GPL-3.0-only */',
                '/* 16 packed DX7 voices; exact complements of FM_FACTORY above. */',
                '#pragma once','#include <stdint.h>',
                '#define ZN_PATCH_BASE 100', '#define ZN_NPATCH 16',
                '#define ZN_SOFT_PAD (ZN_PATCH_BASE + 0)',
                '#define ZN_SOFT_SWELL (ZN_PATCH_BASE + 1)',
                '#define ZN_DEEP_PAD (ZN_PATCH_BASE + 2)']
        for i,name in enumerate(NAME_MAP[3:],3):
            output.append(f'#define ZN_{name} (ZN_PATCH_BASE + {i})')
        output.append('#ifdef ZN_PATCH_DATA')
        output.append('static const uint8_t ZN_PATCH[ZN_NPATCH][128] = {')
        data=[pack(v) for v in bank]
    else: raise ValueError('Expected expanded or packed')
    for n,vals in enumerate(data):
        output.append(f'    {{ /* {NAME_MAP[n]} */')
        for j in range(0,len(vals),16):
            output.append('        '+', '.join(str(x) for x in vals[j:j+16])+',')
        output.append('    },')
    output.extend(['};'])
    if fmt=='packed':output.append('#endif')
    return '\n'.join(output)+'\n'


def main():
    if len(sys.argv)!=3:raise SystemExit('Usage: fm16_bank.py expanded|packed OUTPUT.h')
    Path(sys.argv[2]).write_text(render(sys.argv[1]))

if __name__=='__main__':main()
