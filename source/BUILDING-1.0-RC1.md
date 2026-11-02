# Building 1.0 RC1

Use the JieLi PI32v2 toolchain jieli-linux-toolchains-20260730.1 and the verified AC79 SDK resources used by v17.99.11.
Set JIELI_TOOLCHAIN to the toolchain directory, AC79_SDK to the SDK root, and X0X_FONT_DIRS to your fonts directory.
This release uses DejaVu Sans Condensed Bold substituted as BarlowSemiCondensed-Bold.ttf and BarlowSemiCondensed-SemiBold.ttf, plus DejaVu Sans Condensed as BarlowSemiCondensed-Medium.ttf, matching the previous build.

Hardware:

```sh
sh build.sh --release 17.99.12 --display-version '1.0 RC1'
python3 tests/validate_built_fwsc_rc1.py
```

Host:

```sh
sh build.sh --host --release 17.99.12 --display-version '1.0 RC1'
app/build/host/amb_host tests/pending/rc1-ui.amb screenshots
app/build/host/amb_host tests/pending/wide-ui.amb screenshots
```

The hardware output is app/build/amb-17.99.12.fwsc. The delivered copy is named genAMB-1.0-RC1.fwsc. Numeric package identity FM-1_7179912 is intentionally retained for updater compatibility; the displayed version is 1.0 RC1.
