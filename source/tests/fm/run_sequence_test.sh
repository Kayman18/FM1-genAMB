#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Build the production sequencer harness with a native C compiler.
# --sanitize adds AddressSanitizer and UndefinedBehaviorSanitizer.
set -eu
fm_test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
fm_test_tmp=$(mktemp -d "${TMPDIR:-/tmp}/genamb-fm-sequence.XXXXXX")
trap 'rm -rf "$fm_test_tmp"' EXIT HUP INT TERM
fm_test_cc=${CC:-cc}
case "${FM_TEST_SANITIZE:-0}" in 1) fm_test_sanitize=1 ;; *) fm_test_sanitize=0 ;; esac
case "${1-}" in
    --sanitize) fm_test_sanitize=1; shift ;;
    "") ;;
    *) echo "Usage: $0 [--sanitize]" >&2; exit 2 ;;
esac
if [ "$#" -ne 0 ]; then echo "Usage: $0 [--sanitize]" >&2; exit 2; fi
set -- -std=c11 -Wall -Wextra -Werror -Wno-unused-function -O2
if [ "$fm_test_sanitize" = 1 ]; then
    set -- "$@" -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
fi
"$fm_test_cc" "$@" "$fm_test_dir/sequence_test.c" -o "$fm_test_tmp/sequence_test"
# The production sequencer has no heap allocation. LeakSanitizer is disabled by
# default because restricted Linux runtimes may deny its /proc inspection.
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$fm_test_tmp/sequence_test"
