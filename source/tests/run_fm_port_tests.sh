#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run only the new FM adapter and core risks; no hardware timing claim.
set -eu
cd "$(dirname "$0")/.."
fm_test_out="${1:-app/build/fm-port-tests}"
mkdir -p "$fm_test_out"
fm_test_cc="${CC:-cc}"
python3 app/vendor/zen_fm/gen_factory.py "$fm_test_out/fm_factory.h"
cmp app/fm_port/fm_factory.h "$fm_test_out/fm_factory.h"
python3 app/vendor/zen_fm/gen_fm6_tables.py "$fm_test_out/fm6_tables.h"
cmp app/vendor/zen_fm/fm6_tables.h "$fm_test_out/fm6_tables.h"
python3 app/vendor/zen_fm/zen_patches.py "$fm_test_out/zen_patches.h"
cmp app/vendor/zen_fm/zen_patches.h "$fm_test_out/zen_patches.h"
"$fm_test_cc" -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Werror -Wno-unused-function \
    tests/host/fm_port_test.c -lm -o "$fm_test_out/fm_port_test"
"$fm_test_out/fm_port_test"
"$fm_test_cc" -O2 -ffp-contract=off -std=c99 tests/host/fm_port_reference.c \
    -o "$fm_test_out/fm_port_reference"
# Established against the pinned upstream core before the hardware/UB fixes;
# its 221184 Q24 output samples were also compared byte-for-byte using cmp.
fm_reference_result=$("$fm_test_out/fm_port_reference")
[ "$fm_reference_result" = "8483c408 221184 samples" ]
echo "PASS upstream core PCM fingerprint: $fm_reference_result"
"$fm_test_cc" -O1 -g -ffp-contract=off -std=c99 -fno-omit-frame-pointer \
    -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Wno-unused-function \
    tests/host/fm_port_test.c -lm -o "$fm_test_out/fm_port_sanitize"
# This adapter never allocates. LeakSanitizer cannot inspect /proc tasks in
# the hosted ptrace environment; keep address/undefined checks fully enabled.
ASAN_OPTIONS=detect_leaks=0 "$fm_test_out/fm_port_sanitize"
