#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Native GCC/Clang + GNU-compatible assembler; no device build is touched.
set -eu
fm_test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
fm_test_tmp=$(mktemp -d "${TMPDIR:-/tmp}/genamb-worker-policy.XXXXXX")
trap 'rm -rf "$fm_test_tmp"' EXIT HUP INT TERM
fm_test_cc=${CC:-cc}
fm_test_sanitize=0
case "${1-}" in
    --sanitize) fm_test_sanitize=1; shift ;;
    "") ;;
    *) echo "Usage: $0 [--sanitize]" >&2; exit 2 ;;
esac
if [ "$#" -ne 0 ]; then echo "Usage: $0 [--sanitize]" >&2; exit 2; fi
set -- -std=c11 -O2 -Wall -Wextra -Werror -Wno-unused-function \
    -I"$fm_test_dir/worker_stub" -I"$fm_test_dir/../../app/dsp"
if [ "$fm_test_sanitize" = 1 ]; then
    set -- "$@" -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
fi
"$fm_test_cc" "$@" "$fm_test_dir/worker_test.c" -o "$fm_test_tmp/worker_test"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$fm_test_tmp/worker_test"
