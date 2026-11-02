# Build v17.99.11

Follow BUILDING-v17.99.8.md, using the same JieLi compiler and checksum-verified AC79 SDK resource files. Set JIELI_TOOLCHAIN, AC79_SDK and X0X_FONT_DIRS appropriately. The fonts used for this release are DejaVuSansCondensed-Bold.ttf under both Barlow Bold/SemiBold names and DejaVuSansCondensed.ttf under the Barlow Medium name. Compiler executables must reside on an executable filesystem.

```
sh build.sh --host
mkdir -p app/build/host/screens
app/build/host/amb_host tests/filter/classic-ui.amb app/build/host/screens
app/build/host/amb_host tests/fm/balance-ui.amb app/build/host/screens
app/build/host/amb_host tests/pending/wide-ui.amb app/build/host/screens
sh tests/run_fm_port_tests.sh
sh tests/fm/run_sequence_test.sh --sanitize
sh tests/fm/run_worker_test.sh --sanitize
sh tests/filter/run_classic_tests.sh
sh tests/unitd/run-unitd-tests.sh
sh build.sh --release 17.99.11
python3 tests/validate_built_fwsc_v179911.py app/build/amb-17.99.11.fwsc
```

Host performance counters intentionally do not model FM-1 timing. Use the hardware Performance page before deciding to relocate the filter.
