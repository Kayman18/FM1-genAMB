# Build genAMB v17.99.8

Use the JieLi PI32v2 Linux compiler and verified AC79 SDK inputs described in
BUILDING.md. This release used jieli-linux-toolchains-20260730.1. Dependencies
are external, not bundled. No font files are included.

For exact v17.99.7/v17.99.8 lettering, provide a private font directory with:
BarlowSemiCondensed-SemiBold.ttf -> system DejaVuSansCondensed-Bold.ttf
BarlowSemiCondensed-Bold.ttf -> system DejaVuSansCondensed-Bold.ttf
BarlowSemiCondensed-Medium.ttf -> system DejaVuSansCondensed.ttf
These are the historical substitute mappings, not claims about font identity.

```sh
export JIELI_TOOLCHAIN=/path/to/jieli-linux-toolchains-20260730.1
export AC79_SDK=/path/to/AC79-SDK
export X0X_FONT_DIRS=/path/to/private/font-input-directory
sh build.sh --host
sh tests/filter/run_classic_tests.sh
mkdir -p app/build/host/screens
app/build/host/amb_host tests/filter/classic-ui.amb app/build/host/screens
sh tests/run_fm_port_tests.sh
sh tests/fm/run_worker_test.sh --sanitize
sh tests/fm/run_sequence_test.sh --sanitize
sh tests/unitd/run-unitd-tests.sh
sh build.sh --release 17.99.8
python3 tests/validate_built_fwsc_v17998.py app/build/amb-17.99.8.fwsc
```

Expected identity FM-1_7179908; 610108-byte package. Source modification or a
compiler/font change can change the SHA-256; never substitute a filename check
for the firmware-package validation script. Physical hardware validation is
still required. See RELEASE-v17.99.8-Notes.md for save-format compatibility.
