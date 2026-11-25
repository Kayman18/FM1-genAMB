#!/bin/sh
# Per-scene bit-exactness gate. Each of the nine scenes runs in its own
# process and is checked against its own reference fingerprint, so a change
# that only moves a silent layer's carried-over phase cannot mask - or be
# mistaken for - a change in arithmetic.
set -eu
cd "$(dirname "$0")/../.."
CC="${CC:-cc}"
INC="-Ibase/device -Ibase/include -Ibase/lib -Iapp -Iapp/dsp -Iapp/rings_port -Iapp/fm_port -Iapp/vendor/zen_fm -Iapp/vendor/rings"
OUT=app/build/scene-gates
mkdir -p "$OUT"
for f in app/dsp/amb.c app/fm_port/fm_port.c app/monumatic/reverb.c app/monumatic_reverb_port.c; do
  $CC -O2 -ffp-contract=off -std=c99 -w -DPLAT_HOST $INC -c "$f" -o "$OUT/$(echo "$f" | tr '/' '_' | sed 's/\.c$/.o/')"
done
for f in app/rings_port/rings_port.cc app/vendor/rings/rings/resources.cc \
         app/vendor/rings/rings/dsp/part.cc app/vendor/rings/rings/dsp/resonator.cc \
         app/vendor/rings/rings/dsp/string.cc app/vendor/rings/stmlib/dsp/units.cc \
         app/vendor/rings/stmlib/utils/random.cc; do
  $CC -O2 -ffp-contract=off -w -DPLAT_HOST $INC -c "$f" -o "$OUT/$(echo "$f" | tr '/' '_' | sed 's/\.cc$/.o/')"
done
$CC -O2 -ffp-contract=off -std=gnu11 -w -DPLAT_HOST $INC -c tests/monumatic/genamb_scene_hash.c -o "$OUT/scene.o"
$CC -o "$OUT/scene" "$OUT/scene.o" "$OUT"/app_*.o -lm -lstdc++
# Reference fingerprints: the TEST7 release, each scene in its own process.
set -- \
  "all:fed424d0a43572a9" "pad-only:7c1bb88482f2f6c1" "pad2-only:56c80a2a6cc4c486" \
  "rings-only:7a83bc47b8448383" "fm-only:df9ad0a2b4cfa4cb" "dry:72d403e22e4fac90" \
  "rings-model1:7a83bc47b8448383" "rings-model2:7a83bc47b8448383" "wet-max:77f89b446563173f"
i=0; bad=0
for spec in "$@"; do
  want="${spec#*:}"; name="${spec%%:*}"
  got="$("$OUT/scene" "$i" | awk '{print $3}')"
  if [ "$got" != "$want" ]; then
    printf 'FAIL: scene %s %s expected %s\n' "$name" "$got" "$want"; bad=1
  fi
  i=$((i+1))
done
[ "$bad" -eq 0 ] && echo "PASS: all nine scenes match the TEST7 reference individually" || exit 1
