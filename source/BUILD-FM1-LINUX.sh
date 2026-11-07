#!/usr/bin/env bash
# Current Linux/WSL2 entry point. Dependencies and outputs: BUILDING-RC.md.
set -euo pipefail
cd "$(dirname "$0")"
if [[ "$(uname -m)" != "x86_64" ]]; then
  echo 'The JieLi PI32v2 toolchain requires x86-64 Linux/WSL2.' >&2
  exit 2
fi
exec sh build-rc.sh "$@"
