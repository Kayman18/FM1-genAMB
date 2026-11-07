# genAMB RC source

This is the complete build source for **genAMB RC** on the M-VAVE FM-1, published at [Kayman18/FM1-genAMB](https://github.com/Kayman18/FM1-genAMB). It uses the supplied corrected **TEST3b** audio engine with release labeling and build documentation updated for RC.

| Release field | Value |
| --- | --- |
| Display label | `RC` |
| Numeric updater version | `17.99.32` |
| Firmware identity | `FM-1_7179932` |
| Saved-project format | `23` |
| Installable firmware | [`genAMB-RC.fwsc`](../genAMB-RC.fwsc) |
| Source checkpoint | Corrected TEST3b, audio SRAM placement disabled |

See the [project overview and controls](../README.md), [RC release notes](RELEASE-RC.md), [build instructions](BUILDING-RC.md) and [validation logs](../docs/rc/).

## Build

On x86-64 Linux or WSL2, set `JIELI_TOOLCHAIN` to the directory containing the tested toolchain's `pi32v2/` and `common/` folders, then run:

```sh
sh build-rc.sh
python3 tests/validate_built_fwsc_rc.py app/build/amb-17.99.32.fwsc
```

The build needs Python 3 with Pillow and JieLi's **20260730.1** Linux toolchain. The minimal SDK resources and fonts are included in `build-deps/`. `base/` and `base/bsp/` are included as ordinary source directories, so this distribution does not need a submodule download.

For the desktop simulator:

```sh
sh build-rc.sh --host
```

The device output `app/build/amb-17.99.32.fwsc` is published as `genAMB-RC.fwsc` in the repository root. `app/build/amb.bin` is also published as `genAMB-RC-app.bin` for developers. Use the `.fwsc` package to install the firmware.

## Source layout

| Directory | Contents |
| --- | --- |
| `app/` | Instrument, UI, projects, pad/Rings/FM engines, sequencers and Monumatic reverb |
| `base/` | fm1-base platform and build/install tools, including felucca-bsp |
| `build-deps/` | Pinned SDK packaging resources and bundled font assets |
| `web-editor/` | Local USB-C Web MIDI editor and preset snapshots |
| `tests/` | Package validators, host tests and simulator scenarios |
| `docs/` | Interface documentation, previous captures and archived source README |

## Audio behavior carried forward

The experimental `.dsp_hot` placement is off: `PLAT_HOT_ENABLE` is not defined. The audio path retains the corrected TEST3b placement. Reverb sleep remains enabled at Dry/Wet 0; raising the mix wakes a cleared tank, so the tail blooms in from that point. These are the supplied source's behaviors, preserved in RC. [TEST3 CPU notes](TEST3-CPU-NOTES.md)

RC passed the device code/memory audits, package verification, existing audio hash and sleep/wake gates, the sanitized reverb adapter test, and the full-app Monumatic scenario. Device listening and worst-case hardware timing remain to be checked. The [release notes](RELEASE-RC.md) give the exact results and scope.

## History and attribution

The [previous source README](docs/history/README-before-RC.md), numbered `BUILDING-*`/`RELEASE-*` files, `PENDING_CHANGES.txt` and `NEXT_BUILD_QUEUE.json` record earlier checkpoints. Their version labels and pending-build statements do not describe the current RC. Use `BUILDING-RC.md` and `build-rc.sh` for this release.

The project retains its AMB-1, fm1-base, felucca-bsp, Rings/stmlib, Zen-1/msfa, Hallucigenia, Proteus and UNIT-D attributions. See the [main attribution list](../README.md#attribution-and-licensing) and [component licensing](LICENSING.md). Original notices remain with their source files; missing upstream license texts and font notices have been restored.
