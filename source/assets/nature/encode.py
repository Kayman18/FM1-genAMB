#!/usr/bin/env python3
"""Encode one edited 16 kHz mono PCM16 WAV using the firmware's 516-byte blocks."""
import argparse
import array
import hashlib
import struct
import sys
import wave
from pathlib import Path

STEPS = (7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,
         60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,
         337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,
         1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,
         5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,
         18500,20350,22385,24623,27086,29794,32767)
INDEX = (-1, -1, -1, -1, 2, 4, 6, 8)

def block(pcm):
    samples = list(pcm) + [0] * (1024 - len(pcm))
    predictor = samples[0]
    variation = sum(abs(a-b) for a,b in zip(samples[:63], samples[1:64]))
    index = 0
    while index < 88 and STEPS[index] * 63 < variation:
        index += 1
    data = bytearray(struct.pack('<hBB', predictor,index,0) + bytes(512))
    for i, value in enumerate(samples[1:]):
        step = STEPS[index]
        diff = abs(value - predictor)
        code = 8 if value < predictor else 0
        delta = step >> 3
        for bit, divisor in ((4,0), (2,1), (1,2)):
            part = step >> divisor
            if diff >= part:
                code |= bit
                diff -= part
                delta += part
        predictor = max(-32768, min(32767, predictor + (-delta if code & 8 else delta)))
        index = max(0, min(88,index+INDEX[code & 7]))
        data[4+i//2] |= code << (4*(i&1))
    return data

def encode(path):
    with wave.open(str(path),'rb') as w:
        if (w.getnchannels(), w.getsampwidth(), w.getframerate()) != (1,2,16000):
            raise ValueError('Expected a mono, 16-bit PCM WAV at 16000 Hz')
        frames = w.getnframes()
        if not frames:
            raise ValueError('Empty recording')
        samples = array.array('h', w.readframes(frames))
    if sys.byteorder != 'little':
        samples.byteswap()
    return frames, b''.join(block(samples[i:i+1024]) for i in range(0,frames,1024))

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('wav', type=Path)
    p.add_argument('adpcm', type=Path)
    a = p.parse_args()
    count, data = encode(a.wav)
    a.adpcm.write_bytes(data)
    print(f'frames={count} bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}')
