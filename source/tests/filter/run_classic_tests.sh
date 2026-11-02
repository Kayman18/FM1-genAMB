#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p app/build/host
cc -std=c99 -O1 -g -Wall -Wextra -Werror -ffp-contract=off \
 -fsanitize=address,undefined -fno-sanitize-recover=all \
 tests/filter/classic_filter_test.c -lm -o app/build/host/classic_filter_test
ASAN_OPTIONS=detect_leaks=0 app/build/host/classic_filter_test
