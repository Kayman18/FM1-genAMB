"""Run from the source folder with python3 tests/nature/run-va.py."""
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[2]
out=root/'app/build/va-tests'
out.mkdir(parents=True,exist_ok=True)
def run(*args):
    return subprocess.check_output(args,cwd=root,text=True)
run('sh','build-rc.sh','--host')
for name in ['migration','va-bank','lfo-six']:
    print(run('app/build/host/amb_host',f'tests/nature/{name}.amb',out))
probe=out/'va-routes'
run('cc','-O2','-Iapp/dsp','-Iapp/fm_port','tests/nature/va_routes.c',
    *sorted((root/'app/build/host/o2').glob('*.o')),'-lm','-lstdc++','-o',probe)
def render(preset,dest=0,route=0):
    return run(probe,str(preset),str(dest),str(route)).strip()
assert len({render(p) for p in range(41)})==41
for route in range(6):
    for dest in range(15,25):
        # Pulse Bed reaches decay and sustain within the probe's held-note time.
        preset=22 if dest in (22,23) else 0
        assert render(preset,0,route)!=render(preset,dest,route),(dest,route)
print('PASS 41 distinct audible renders, four voices, note release, and 60 matrix/PCM comparisons')
for mode in range(4):
    print(render(1,0,6+mode))
print(run('python3','tests/web_editor_metadata_test.py'))
