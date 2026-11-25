"""Verify audible LFO routing through the production mixer and signed depths."""
from pathlib import Path
import subprocess
import sys
probe=Path(sys.argv[1]).resolve()
def render(base, routes=(0,0,0)):
    data=subprocess.check_output([probe,str(base),*map(str,routes)],text=True).split()
    return data[0],float(data[1])
baseline=render(50)
assert baseline[1]>0
for i in range(3):
    for amount,expected in [(100,95),(-100,5)]:
        routes=[0,0,0]
        routes[i]=amount
        assert render(50,routes)==render(expected)
assert render(50,[100,-100,0])==baseline
assert render(50,[100,100,100])==render(100)
assert render(50,[-100,-100,-100])[1]==0
assert render(0,[100,100,100])==render(100)
assert render(0)[1]==0
print('PASS M7/M8/M9 positive and negative depths in actual FM audio, cancellation, summed limits, silent unassigned zero and opening a zero base')

def cycle(depth):
    return list(map(float,subprocess.check_output([probe,'50',str(depth),'0','0','cycle'],text=True).split()))
flat, modulated=cycle(0),cycle(50)
ratios=[b/a for a,b in zip(flat,modulated)]
assert ratios[0]>2 and ratios[1]<0.3 and ratios[2]>2, ratios
print('PASS repeating pulse LFO drives FM above, below and back above the unchanged mixer level')

def timbre(routes=(0,0,0), dest=16, depth=0, alg=5):
    args=[probe,'50',*map(str,routes),str(dest),str(depth),str(alg)]
    return subprocess.check_output(args,text=True).split()[0]

for route in range(3):
    for sign in [-1,1]:
        r=[0,0,0]
        r[route]=100*sign
        assert timbre(r,16)==timbre(depth=100*sign)!=timbre()
        expected=max(1,min(32,15+16*sign))
        assert timbre(r,17,alg=15)==timbre(alg=expected)!=timbre(alg=15)
assert timbre([100,-100,0])==timbre()
assert timbre([100,100,100],17)==timbre(alg=32)
assert timbre([-100,-100,-100],17)==timbre(alg=1)
assert timbre(depth=-100,alg=32)==timbre(depth=100,alg=32)
print('PASS all three LFOs drive FM Depth and latched Algorithm in rendered audio, signed routing, clipping and all-carrier independence')
