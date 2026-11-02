#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p app/build/host
cc -O1 -g -std=c11 -Wall -Wextra -Werror -Wno-unused-function -DPLAT_HOST \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 tests/unitd/unitd_core_test.c -lm -o app/build/host/unitd_core_test
ASAN_OPTIONS=detect_leaks=0 app/build/host/unitd_core_test
