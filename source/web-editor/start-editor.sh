#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
python3 -m http.server 8765 --bind 127.0.0.1 >/dev/null 2>&1 &
pid=$!
trap 'kill $pid 2>/dev/null || true' EXIT
url='http://127.0.0.1:8765/'
opened=0
for browser in microsoft-edge-stable microsoft-edge google-chrome-stable google-chrome chromium chromium-browser; do
  if command -v "$browser" >/dev/null 2>&1; then
    "$browser" "$url" >/dev/null 2>&1 &
    opened=1
    break
  fi
done
if [ "$opened" -eq 0 ]; then
  if command -v xdg-open >/dev/null 2>&1; then xdg-open "$url" >/dev/null 2>&1 || true
  elif command -v open >/dev/null 2>&1; then open "$url" || true
  fi
fi
echo "genAMB FM-1 USB-C Editor: $url"
echo "Chrome/Edge/Chromium with Web MIDI SysEx is recommended."
echo "Press Ctrl+C to stop."
wait $pid
