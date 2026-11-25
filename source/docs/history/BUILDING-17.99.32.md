# Building genAMB RC

The published RC is built from the corrected TEST3b source, with numeric version **17.99.32**, display label **RC**, and updater identity **FM-1_7179932**. The source includes `base/`, `base/bsp/`, the fonts and the three SDK resources used to package the firmware.

## Prerequisites

- x86-64 Linux or WSL2 for the device build.
- JieLi Linux toolchain **20260730.1**, containing `pi32v2/` and `common/`. The compiler identifies as clang 4.0.1.
- Python 3 with Pillow for source and font generation.
- A host C/C++ compiler and development libraries for the optional desktop simulator and host tests.

Set the toolchain location to its extracted directory:

```sh
export JIELI_TOOLCHAIN=/path/to/jieli-linux-toolchains-20260730.1
```

The build uses `build-deps/sdk/` for `uboot.boot`, `cfg_tool.bin` and `cfg/eq_cfg_hw.bin`, and `build-deps/fonts/` with the DUSK font set. `AC79_SDK`, `X0X_FONT_DIRS`, `X0X_FONTSET` and `PYTHON` can override those defaults. Keep the bundled resources and DUSK font set when reproducing this RC.

The toolchain is an external dependency. `base/tools/get_toolchain.sh` downloads the upstream toolchain, but that download URL can advance to a newer release; use **20260730.1** to reproduce the compiler used here.

## Device build

Run from the repository's `source/` directory:

```sh
sh build-rc.sh
python3 tests/validate_built_fwsc_rc.py app/build/amb-17.99.32.fwsc
```

`build-rc.sh` selects `--release 17.99.32 --display-version RC`. `BUILD-FM1-LINUX.sh` is a Linux/WSL2 wrapper for the same entry point. A plain `sh build.sh` also defaults to this RC; use the explicit RC wrapper when following release instructions.

| Build output | Published file | Purpose |
| --- | --- | --- |
| `app/build/amb-17.99.32.fwsc` | `../genAMB-RC.fwsc` | Complete installable update package; 610,108 bytes |
| `app/build/amb.bin` | `../genAMB-RC-app.bin` | Raw app component for developers; 304,860 bytes |
| `app/build/loader/ota.bin` | Included inside the `.fwsc` | Update-loader component; 6,952 bytes |

`app/build/` contains generated files. The published firmware is copied from the device build; the raw app alone is not a complete installer package.

The normal build checks `.ram_text` call safety, the 64-bit-multiply CPU-erratum pattern, soft-double routines, image and RAM budgets, and MMIO placement. The package validator checks the numeric identity, header/table/payload CRCs, the loader, decrypted application bytes and erased slot padding.

## Preserve the TEST3b configuration

Leave **`PLAT_HOT_ENABLE` undefined**. The earlier TEST3 experiment moved audio code into `.dsp_hot` in executable SRAM and faulted on hardware. Corrected TEST3b and this RC leave that section at zero bytes; the standard flash-driver `.ram_text` remains present.

Keep the default **`GENAMB_REVERB_SLEEP=1`**. At Dry/Wet 0, the inaudible tank sleeps; when the mix rises, the tank is cleared and a fresh reverb tail develops. Changing this option changes the supplied TEST3b behavior. The [TEST3 CPU notes](TEST3-CPU-NOTES.md) record the original comparison and its host-measurement limits.

## Host build and focused checks

These commands run from `source/`:

```sh
sh tests/monumatic/run_cpu_gates.sh
sh tests/monumatic/run_adapter_test.sh
sh build-rc.sh --host
mkdir -p app/build/rc-integration
app/build/host/amb_host tests/monumatic/integration.amb app/build/rc-integration
```

The CPU gates check nine seeded render configurations against `GAGOLDEN 963138e825b936e9` and exercise reverb sleep/wake. The adapter test uses AddressSanitizer and UndefinedBehaviorSanitizer. The full-app scenario checks FX navigation, controls, persistence across reboot, maximum-wet audio and panic silence. Current results are in [`../docs/rc/`](../docs/rc/) and summarized in [RELEASE-RC.md](RELEASE-RC.md).

The older `tests/run_tests.sh` targets historical engine/scenario layouts, including absent `tests/scenarios/*.amb` files. It is retained as history and is not the RC validation command. The focused checks above are the checks run for this release.

## Verify or install the published package

From the repository root, verify the downloaded artifacts with:

```sh
sha256sum -c SHA256SUMS.txt
```

For the included USB-MIDI installer, install its Python dependencies (`mido` and `python-rtmidi`) and run from `source/`:

```sh
python3 base/tools/fm1_install.py ../genAMB-RC.fwsc
```

Physical FM-1 timing, dropouts and listening still need to be evaluated on the device. Host render speed is not an FM-1 CPU measurement.
