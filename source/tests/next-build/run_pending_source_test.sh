#!/bin/sh
# Native source validation only; does not build an app or device firmware.
set -eu
pending_test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
pending_test_root=$(CDPATH= cd -- "$pending_test_dir/../.." && pwd)
pending_test_tmp=$(mktemp -d "${TMPDIR:-/tmp}/genamb-pending.XXXXXX")
trap 'rm -rf "$pending_test_tmp"' EXIT HUP INT TERM
pending_test_cc=${CC:-cc}
"$pending_test_cc" -std=c11 -DEN_HOST -Wall -Wextra -Werror \
    -Wno-unused-function -Wno-unused-variable -Wno-misleading-indentation \
    -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
    -fno-omit-frame-pointer -ffp-contract=off \
    -I"$pending_test_root/app/dsp" -I"$pending_test_root/app/rings_port" \
    -I"$pending_test_root/app/fm_port" -I"$pending_test_root/base/include" \
    "$pending_test_dir/pending-source-test.c" -o "$pending_test_tmp/test"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$pending_test_tmp/test"
