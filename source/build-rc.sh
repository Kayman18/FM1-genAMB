#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ "${1:-}" = "--buffer-512" ]; then
    shift
    export FM1B_AUDIO_FRAMES=512
    exec sh build.sh --release 17.99.57 --display-version T14-512 "$@"
fi
export FM1B_AUDIO_FRAMES=256
exec sh build.sh --release 17.99.56 --display-version T14-256 "$@"
