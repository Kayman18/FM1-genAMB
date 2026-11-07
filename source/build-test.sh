#!/bin/sh
# Compatibility entry point: build the current RC release.
set -eu
cd "$(dirname "$0")"
exec sh build-rc.sh "$@"
