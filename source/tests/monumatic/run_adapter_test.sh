#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p app/build/monumatic
cc -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -DPLAT_HOST -Iapp -Ibase/include tests/monumatic/adapter_test.c app/monumatic_reverb_port.c app/monumatic/reverb.c -lm -o app/build/monumatic/adapter_test
ASAN_OPTIONS=detect_leaks=0 app/build/monumatic/adapter_test
