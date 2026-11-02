#!/usr/bin/env bash
# genAMB v17.99.3 — build on Linux x86-64 (or WSL2) with the real JieLi toolchain.
set -euo pipefail
cd "$(dirname "$0")"
if [[ "$(uname -m)" != "x86_64" ]]; then
  echo 'Requires x86-64 Linux/WSL2 for JieLi pi32v2 toolchain' >&2
  exit 2
fi
if [[ ! -x "${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}/pi32v2/bin/clang" ]]; then
  echo 'JieLi toolchain missing; downloading public Linux release.'
  sh base/tools/get_toolchain.sh "$HOME/.jieli"
  export JIELI_TOOLCHAIN="$HOME/.jieli/toolchain"
fi
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
if [[ ! -f "$AC79_SDK/cpu/wl82/tools/uboot.boot" ]]; then
  echo 'AC79 SDK missing; cloning the tested public tag.'
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git "$AC79_SDK"
fi
# Fonts are regenerated from the public fm1-base source at build time; they
# are intentionally not embedded in this redistributable archive.
if [[ ! -f base/assets/fonts/BarlowSemiCondensed-SemiBold.ttf || ! -f base/assets/fonts/BarlowSemiCondensed-Medium.ttf || ! -f base/assets/fonts/BarlowSemiCondensed-Bold.ttf ]]; then
  echo 'Restoring build-only font assets from the public fm1-base repository.'
  temp_repo="$(mktemp -d)"
  trap 'rm -rf "$temp_repo"' EXIT
  git clone --depth 1 https://github.com/charlesvestal/fm1-base.git "$temp_repo/fm1-base"
  mkdir -p base/assets/fonts
  cp "$temp_repo/fm1-base"/assets/fonts/BarlowSemiCondensed-*.ttf base/assets/fonts/
fi
python3 -c 'from PIL import Image; print("Pillow: OK")'
sh build.sh --release 17.99.3
python3 tests/validate_built_fwsc.py app/build/amb-17.99.3.fwsc
mkdir -p ./firmware-output
cp app/build/amb-17.99.3.fwsc ./firmware-output/genAMB-v17.99.3.fwsc
sha256sum ./firmware-output/genAMB-v17.99.3.fwsc
printf 'READY: %s/firmware-output/genAMB-v17.99.3.fwsc\n' "$PWD"
