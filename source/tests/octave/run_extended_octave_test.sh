#!/bin/sh
# Native source validation only; does not build an app or device firmware.
set -eu
octave_test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
octave_test_root=$(CDPATH= cd -- "$octave_test_dir/../.." && pwd)
octave_test_tmp=$(mktemp -d "${TMPDIR:-/tmp}/genamb-octave.XXXXXX")
trap 'rm -rf "$octave_test_tmp"' EXIT HUP INT TERM
octave_test_cc=${CC:-cc}
"$octave_test_cc" -std=c11 -DEN_HOST -Wall -Wextra -Werror \
    -Wno-unused-function -Wno-unused-variable -Wno-misleading-indentation \
    -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
    -fno-omit-frame-pointer -ffp-contract=off \
    -I"$octave_test_root/app/dsp" -I"$octave_test_root/app/rings_port" \
    -I"$octave_test_root/app/fm_port" -I"$octave_test_root/base/include" \
    "$octave_test_dir/extended-octave-test.c" -o "$octave_test_tmp/test"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$octave_test_tmp/test"
