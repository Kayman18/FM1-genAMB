#!/bin/sh
# genAMB RC: corrected TEST3b DSP, with executable-SRAM placement disabled.
set -eu
cd "$(dirname "$0")"
exec sh build.sh --release 17.99.32 --display-version RC "$@"
