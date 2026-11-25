#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ "${1:-}" = "--buffer-512" ]; then
    shift
    export FM1B_AUDIO_FRAMES=512
    exec sh build.sh --release 17.99.47 --display-version 1.0-512 "$@"
fi
export FM1B_AUDIO_FRAMES=256
exec sh build.sh --release 17.99.46 --display-version 1.0-256 "$@"
