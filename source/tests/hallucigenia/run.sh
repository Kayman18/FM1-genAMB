#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
# Compile the unmodified upstream Module portion with a small Rack API shim.
python3 - <<'PY'
from pathlib import Path
s=Path('app/vendor/hallucigenia/Darwinism.cpp').read_text()
s=s.split('struct DarwinismWidget')[0].replace('#include "plugin.hpp"','')
Path('tests/hallucigenia/upstream_module.inc').write_text(s)
PY
hall_tmp=$(mktemp -d)
trap 'rm -rf "$hall_tmp"; rm -f tests/hallucigenia/upstream_module.inc' EXIT
c++ -std=c++11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all tests/hallucigenia/oracle.cpp -o "$hall_tmp/oracle"
ASAN_OPTIONS=detect_leaks=0 "$hall_tmp/oracle"
