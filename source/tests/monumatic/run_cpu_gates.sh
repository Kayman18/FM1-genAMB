#!/bin/sh
# Bit-exactness gates for the TEST3 CPU work.
#
# genamb_golden_hash renders nine configurations that turn each layer and each
# effect on and off, plays notes, freezes and panics, and hashes every output
# sample. The expected value is the pre-optimization output, so any change in
# arithmetic (as opposed to speed) shows up here.
#
# genamb_reverb_wake exercises the reverb sleep/wake transition and requires
# finite audio on both sides of every DRY/WET change.
set -eu
cd "$(dirname "$0")/../.."
CC="${CC:-cc}"
EXPECT="GAGOLDEN 963138e825b936e9"
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
echo "PASS: nine-configuration engine render hash unchanged ($got)"
"$OUT/wake" | tail -1 | grep -q "WAKE OK" || { echo "FAIL: reverb sleep/wake left no audio"; exit 1; }
echo "PASS: reverb sleep/wake keeps finite audio across every DRY/WET change"
