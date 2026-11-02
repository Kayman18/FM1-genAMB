#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
cc -O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/filter/filter_model_test.c -lm -o app/build/host/filter_model_test
ASAN_OPTIONS=detect_leaks=0 app/build/host/filter_model_test
