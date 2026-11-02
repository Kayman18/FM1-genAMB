# Building genAMB v17.99.7

Requirements: JieLi PI32v2 Linux toolchain (e.g. `jieli-linux-toolchains-20260730.1.tar.xz`) and AC79 SDK reference boot files (uboot.boot, cfg_tool.bin, eq_cfg_hw.bin) matching the checksum table in `base/tools/build.py`, Python 3, Pillow, and the three licensed Barlow SemiCondensed TTF inputs required by `base/tools/gen_font.py`.

This source archive does **not** redistribute the original Barlow font binaries, the proprietary JieLi compiler, or the AC79 SDK resources. They must be supplied separately. The generating source references their locations in `base/assets/fonts/` or via `X0X_FONT_DIRS`.

```sh
export JIELI_TOOLCHAIN=/path/to/jieli-linux-toolchains-20260730.1
export AC79_SDK=/path/to/verified/AC79-SDK
sh build.sh --host
sh build.sh --release 17.99.7
python3 tests/validate_built_fwsc_v17997.py app/build/amb-17.99.7.fwsc
```

The generated firmware should be 610,108 bytes and use product identity `FM-1_7179907`. See `RELEASE-v17.99.7-Notes.md` for tested resources, changes and font reproduction caveat.
