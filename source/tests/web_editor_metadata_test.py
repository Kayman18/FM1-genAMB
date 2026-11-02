#!/usr/bin/env python3
from pathlib import Path
import json,re
ROOT=Path(__file__).resolve().parents[1]
h=(ROOT/'app/dsp/amb.h').read_text()
html=(ROOT/'web-editor/index.html').read_text()
app=(ROOT/'app/app.c').read_text()
body=re.search(r'enum \{(.*?)P_NPARAMS\s*\n\};',h,re.S).group(1)
keys=[]
for k in re.findall(r'\b(P_[A-Z0-9]+)\b',body):
    if k not in keys: keys.append(k)
params=json.loads(re.search(r'const PARAMS=(\[.*?\]);\nconst PIECES=',html,re.S).group(1))
assert len(keys)==167==len(params)
assert [p['id'] for p in params]==list(range(167))
assert [p['key'] for p in params]==keys
assert len({p['key'] for p in params})==167
for p in params:
    assert p['lo'] <= p['def'] <= p['hi'],p
assert '#define GA_PARAM_CHUNK 32u' in app
assert 'static uint8_t ga_tx[80];' in app
assert 'GA_SET_PARAM_BLOCK' in app and 'GA_SET_META' in app
assert 'APP_SYSEX' in (ROOT/'app/app.h').read_text()
assert "format:'genAMB-FM1-Preset'" in html
assert 'SAVE .GENAMB' in html and 'LOAD .GENAMB' in html
assert 'start+=32' in html and 'Math.min(32' in html
assert 'if(stateWait)return stateWait.promise' in html
print('PASS web editor metadata: 167/167 exact parameter IDs, 32-param SysEx chunks, snapshot save/load present')
