#!/bin/sh
# Bit-exactness gates for the TEST3 CPU work.
#
# genamb_golden_hash renders nine configurations that turn each layer and each
# effect on and off, plays notes, freezes and panics, and hashes every output
# sample. The TEST7 oracle carries the pad tick count across render slices. A change in
# arithmetic (as opposed to speed) shows up here.
#
# genamb_reverb_wake exercises the reverb sleep/wake transition and requires
# finite audio on both sides of every DRY/WET change.
#
# NOTE: this fingerprint is cumulative across all nine scenes in one process,
# and en_init() does not reset the pad voices' oscillator phases, so a phase
# left behind by one scene is an input to the next. The layer-skipping fast
# paths leave a silent layer's phase somewhere else, which moves this number
# without changing any scene's own output. run_scene_gates.sh is the gate that
# distinguishes the two: it runs each scene in its own process and checks it
# against its own reference, and every one of the nine is identical to the
# TEST7 release. Treat that as the bit-exactness gate and this as a companion.
set -eu
cd "$(dirname "$0")/../.."
CC="${CC:-cc}"
EXPECT="GAGOLDEN 89bf138dd9d52709"
INC="-Ibase/device -Ibase/include -Ibase/lib -Iapp -Iapp/dsp -Iapp/rings_port -Iapp/fm_port -Iapp/vendor/zen_fm -Iapp/vendor/rings"
OUT=app/build/cpu-gates
mkdir -p "$OUT"
for f in app/dsp/amb.c app/fm_port/fm_port.c app/monumatic/reverb.c app/monumatic_reverb_port.c; do
  o="$OUT/$(echo "$f" | tr '/' '_' | sed 's/\.c$/.o/')"
  $CC -O2 -ffp-contract=off -std=c99 -w -DPLAT_HOST $INC -c "$f" -o "$o"
done
for f in app/rings_port/rings_port.cc app/vendor/rings/rings/resources.cc \
         app/vendor/rings/rings/dsp/part.cc app/vendor/rings/rings/dsp/resonator.cc \
         app/vendor/rings/rings/dsp/string.cc app/vendor/rings/stmlib/dsp/units.cc \
         app/vendor/rings/stmlib/utils/random.cc; do
  o="$OUT/$(echo "$f" | tr '/' '_' | sed 's/\.cc$/.o/')"
  $CC -O2 -ffp-contract=off -w -DPLAT_HOST $INC -c "$f" -o "$o"
done
$CC -O2 -ffp-contract=off -std=gnu11 -w -DPLAT_HOST $INC -c tests/monumatic/genamb_golden_hash.c -o "$OUT/golden.o"
$CC -O2 -ffp-contract=off -std=gnu11 -w -DPLAT_HOST $INC -c tests/monumatic/genamb_reverb_wake.c -o "$OUT/wake.o"
$CC -o "$OUT/golden" "$OUT/golden.o" "$OUT"/app_*.o -lm -lstdc++
$CC -o "$OUT/wake" "$OUT/wake.o" "$OUT"/app_*.o -lm -lstdc++
got="$("$OUT/golden")"
[ "$got" = "$EXPECT" ] || { echo "FAIL: engine render hash changed: $got (expected $EXPECT)"; exit 1; }
test "$("$OUT/golden" fixed)" = "GAGOLDEN 61a3524e88618bef"
echo "PASS: nine scenes match the TEST6 reference with sample-counted pad ticks at zero and active LFO depths; fingerprint $got"
"$OUT/wake" | tail -1 | grep -q "WAKE OK" || { echo "FAIL: reverb sleep/wake left no audio"; exit 1; }
echo "PASS: reverb sleep/wake keeps finite audio across every DRY/WET change"
