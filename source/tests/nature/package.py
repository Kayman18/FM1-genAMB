#!/usr/bin/env python3
"""Check packaged app, stream bounds, unique assets, declared sizes and the updater."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[2]
assets = root / 'assets/nature'
app = (root / 'app/build/amb.bin').read_bytes()
version = sys.argv[1] if len(sys.argv) > 1 else "17.99.56"
if version not in ("17.99.56", "17.99.57"):
    raise SystemExit("expected version 17.99.56 (256) or 17.99.57 (512)")
subprocess.run([sys.executable, str(root / 'tests/validate_built_fwsc_rc.py'),
                str(root / f'app/build/amb-{version}.fwsc'), version], check=True)
rows = json.loads((assets / 'manifest.json').read_text())
for row in rows:
    b = (assets / (row['name'] + '.adpcm')).read_bytes()
    assert hashlib.sha256(b).hexdigest() == row['sha256']
    assert len(b) == row['bytes'] == (row['frames'] + 1023) // 1024 * 516
    assert app.count(b) == 1, row['name']
assert sum(r['bytes'] for r in rows) == 219816
print('PASS eight original assets and normalized Rain embedded exactly once (219816 bytes)')

# The three TEST2 additions must reside in the wave bank and occur exactly once.
wave = (assets/'animal-wave.bin').read_bytes()
extra = json.loads((assets/'animal-wave.json').read_text())
assert len(extra) == 3
for row in extra:
    start = row['offset']
    audio = wave[start:start+row['bytes']]
    assert len(audio) == ((row['frames']+1023)//1024)*516 == row['bytes']
    assert hashlib.sha256(audio).hexdigest() == row['sha256']
    assert hashlib.sha256((assets/'recordings'/row['recording']).read_bytes()).hexdigest() == row['recording_sha256']
    assert app.count(audio) == 0 and wave.count(audio) == 1
assert len(wave) == 28*4096 and sum(r['bytes'] for r in extra) == 111972
print('PASS Finch 1, Finch 2 and Squirrel 3 occur exactly once externally, with full source recordings verified')
