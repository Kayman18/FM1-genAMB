# Building genAMB v17.99.6 for the M-VAVE FM-1

This source archive excludes proprietary JieLi SDK files and compiler executables,
and excludes upstream font binaries. It does include the fixed-size UNIT-D
source, the entire 16-preset FM bank data, minimal freestanding C++ compatibility
headers, release-package validation, and host tests.

Prerequisites:

1. The official `jieli-linux-toolchains-20260730.1.tar.xz` unpacked to a
   directory containing `pi32v2/bin/clang` and `common/bin/clang`.
2. The legally obtained AC79 SDK resource tree including
   `cpu/wl82/tools/uboot.boot`, `cpu/wl82/tools/cfg_tool.bin` and
   `cpu/wl82/tools/cfg/eq_cfg_hw.bin`, matching the hashes in
   `base/tools/build.py`.
3. The Barlow Semi Condensed Medium, SemiBold, and Bold font files (from their
   official source, under the license in `base/assets/fonts/Barlow-OFL.txt`),
   placed under `base/assets/fonts/` with the expected original filenames.
4. Python 3, host `cc`/`make`, and a Unix shell.

Build and validate:

```sh
export JIELI_TOOLCHAIN=/path/to/jieli-linux-toolchains-20260730.1
export AC79_SDK=/path/to/AC79-SDK-resources
./build.sh --host
./build.sh --release 17.99.6
python3 tests/validate_built_fwsc_v17996.py app/build/amb-17.99.6.fwsc
sh tests/unitd/run-unitd-tests.sh
sh tests/fm/run_worker_test.sh
sh tests/fm/run_sequence_test.sh
sh tests/run_fm_port_tests.sh
sh tests/filter/run_filter_test.sh
```

The flashable firmware is generated as `app/build/amb-17.99.6.fwsc`.
UNIT-D is an adaptation of the graph sequencer rather than an FM sound engine:
Root/Chord follows the active Original progression, while Rings and FM have
independent Source/Options settings and polyphonic graph walkers.
