#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out=app/build/nature-tests
mkdir -p "$out"
python3 assets/nature/generate.py --check
python3 tests/web_editor_metadata_test.py
cc -std=c99 -O2 -ffp-contract=off -DPLAT_HOST -Iapp/fm_port -Ibase/include tests/nature/stereo_filter.c app/fm_port/fm_filter.c -o "$out/stereo-filter"
"$out/stereo-filter"
sh tests/monumatic/run_cpu_gates.sh
cc -O2 -std=c99 -Iapp/dsp tests/nature/fm_volume.c app/build/cpu-gates/app_*.o -lm -lstdc++ -o "$out/fm-volume"
python3 tests/nature/fm_routes.py "$out/fm-volume"
cc -std=c99 -O2 -ffp-contract=off -Iapp/dsp -Ibase/include tests/nature/player.c -lm -o "$out/player"
"$out/player"
cc -std=c99 -O2 -Iapp/dsp tests/nature/mixer.c app/build/cpu-gates/app_*.o -lm -lstdc++ -o "$out/mixer"
"$out/mixer"
sh build-rc.sh --host
app/build/host/amb_host tests/nature/pad-timing.amb "$out"
app/build/host/amb_host tests/nature/controls.amb "$out"
app/build/host/amb_host tests/nature/seq-pages.amb "$out"
app/build/host/amb_host tests/nature/fm-route-controls.amb "$out"
app/build/host/amb_host tests/nature/migration.amb "$out"
cc -O2 -std=c99 -Iapp/dsp tests/nature/lfo_pair.c app/build/cpu-gates/app_*.o -lm -lstdc++ -o "$out/lfo-pair"
for mode in tone random -1 0 1 2 3; do "$out/lfo-pair" "$mode"; done
app/build/host/amb_host tests/nature/lfo-speed-seq.amb "$out"

off=$("$out/lfo-pair" dest 0)
for dest in $(seq 1 17); do
    on=$("$out/lfo-pair" dest "$dest")
    [ "$off" != "$on" ] || { echo "No PCM difference on LFO destination $dest"; exit 1; }
done
echo 'PASS all 17 assignable destinations change rendered PCM against an unassigned route'
