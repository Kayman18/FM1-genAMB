#!/usr/bin/env python3
"""Generate the TEST1 Rain payload from the preserved PCM master.

The target is the measured RMS of the decoded River loop at equal source
levels. A soft knee on rare drops protects headroom while retaining the
weather envelope. The processing is performed offline.
"""
from array import array
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import wave
from encode import block, STEPS, INDEX

ROOT = Path(__file__).resolve().parent

def measure(data, frames):
    total, peak, count = 0, 0, 0
    for start in range(0, len(data), 516):
        pred, index, _ = struct.unpack_from('<hBB', data, start)
        for i in range(min(1024, frames-count)):
            if i:
                code=(data[start+4+(i-1)//2] >> (4*((i-1)&1))) & 15
                step=STEPS[index]
                delta=(step>>3) + (step if code&4 else 0) + (step>>1 if code&2 else 0) + (step>>2 if code&1 else 0)
                pred=max(-32768,min(32767,pred+(-delta if code&8 else delta)))
                index=max(0,min(88,index+INDEX[code&7]))
            peak=max(peak,abs(pred))
            total+=pred*pred
            count+=1
    return 10*math.log10(total/count/(32768**2)), 20*math.log10(peak/32768)

def prepare(samples, gain):
    knee, room = 0.60, 0.28
    output=array('h')
    for sample in samples:
        x=sample*gain/32768
        a=abs(x)
        if a>knee:
            a=knee+room*(a-knee)/(room+a-knee)
        output.append(round(math.copysign(a,x)*32767))
    return b''.join(block(output[i:i+1024]) for i in range(0,len(output),1024))

def main():
    rows=json.loads((ROOT/'manifest.json').read_text())
    water=rows[0]
    target,_=measure((ROOT/'water.adpcm').read_bytes(),water['frames'])
    with wave.open(str(ROOT/'masters/Rain-16k-mono.wav'),'rb') as w:
        assert (w.getframerate(),w.getnchannels(),w.getsampwidth())==(16000,1,2)
        samples=array('h',w.readframes(w.getnframes()))
    if sys.byteorder!='little':samples.byteswap()
    gain=1.0
    for _ in range(8):
        data=prepare(samples,gain)
        rms,peak=measure(data,len(samples))
        if abs(target-rms)<0.015: break
        gain*=10**((target-rms)/20)
    assert abs(target-rms)<0.03 and peak < -0.6
    rain=rows[1]
    rain.update(sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),
                frames=len(samples),normalized_rms_dbfs=round(rms,3),
                normalized_peak_dbfs=round(peak,3))
    (ROOT/'rain.adpcm').write_bytes(data)
    (ROOT/'manifest.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(f'River RMS {target:.3f} dBFS, Rain RMS {rms:.3f} dBFS, Rain peak {peak:.3f} dBFS.')
if __name__=='__main__':
    main()
