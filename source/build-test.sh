#!/bin/sh
set -eu
cd "$(dirname "$0")"
exec sh build.sh --release 17.99.31 --display-version 'DUSK MONU2' "$@"
