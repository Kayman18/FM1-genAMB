# genAMB RC — release notes

**genAMB RC** publishes the corrected **TEST3b** source and freshly built FM-1 firmware. The updater version advances from 17.99.31 to **17.99.32**, with display label **RC** and firmware identity **FM-1_7179932**. The source checkpoint is the repository's **RC** release branch.

## Downloads

| File | Use |
| --- | --- |
| [`genAMB-RC.fwsc`](../genAMB-RC.fwsc) | Complete firmware update package for installation |
| [`genAMB-RC-app.bin`](../genAMB-RC-app.bin) | Raw application component for developers |
| [`genAMB-RC-Web-Editor.zip`](../genAMB-RC-Web-Editor.zip) | Local USB-C Web MIDI editor |
| [RC source ZIP](https://github.com/Kayman18/FM1-genAMB/archive/refs/heads/RC.zip) | Source, build scripts, dependencies and validation records |
| [`SHA256SUMS.txt`](../SHA256SUMS.txt) | Published artifact checksums |

The prior TEST2 firmware and editor remain in the repository for rollback.

## What carries forward from TEST3b

The synth engines, sequencers, filters, mixer, Monumatic stereo reverb, control mapping and project format **23** are retained from the uploaded source. RC updates the release identity, display/editor labels, publication metadata and build documentation.

The TEST3b CPU changes remain active:

- Removed calculations for dead reverb sends that the stereo reverb overwrote before use.
- Reverb sleep at Dry/Wet 0, with the tank cleared when the mix rises. A new tail blooms in on wake; the hidden tail does not continue evolving while muted.
- The experimental audio-code SRAM placement stays **disabled**. The `.dsp_hot` section is zero bytes in the RC binary.

The [TEST3 CPU notes](TEST3-CPU-NOTES.md) preserve the original performance analysis. Their 18.2% figure describes host instruction savings in the fully dry case, not a measured FM-1 CPU improvement. No new DSP changes are introduced by the RC labeling.

## Device build

Built with JieLi Linux toolchain **20260730.1**, clang **4.0.1**, using the bundled SDK resources and DUSK fonts.

| Item | Result |
| --- | --- |
| Installable package | 610,108 bytes |
| Application image | 304,860 bytes |
| Update loader | 6,952 bytes |
| Normal RAM (`.data` + `.bss`) | 96,980 / 98,304 bytes |
| Pool memory | 294,248 / 344,064 bytes |
| Audio SRAM section (`.dsp_hot`) | 0 bytes; start and end both `0x01c00b98` |
| Display identifier in target binary | `AMB-1 RC` |

The build passed `.ram_text` checks (929 instructions, no calls), the 64-bit-multiply audit, soft-double checks, image and memory budgets, and the check that register access remains in the BSP. Four inherited unused-typedef warnings are recorded in the [build log](../docs/rc/build.log).

## Validation

| Check | Result and evidence |
| --- | --- |
| Package structure and contents | PASS: expected identity, header/table/payload CRCs, loader, decrypted app and padding; [log](../docs/rc/package-validation.log) |
| Audio placement and memory | PASS: `.dsp_hot` is empty; [memory layout](../docs/rc/memory-layout.json) |
| Nine-configuration engine output | PASS: `GAGOLDEN 963138e825b936e9`; [CPU gates](../docs/rc/cpu-gates.log) |
| Reverb sleep/wake | PASS: finite output across Dry/Wet transitions; [CPU gates](../docs/rc/cpu-gates.log) |
| Sanitized reverb adapter | PASS: ASan/UBSan, 4,580,351 frames, peak `0.0331464`, energy `1.33899`; [log](../docs/rc/adapter-test.log) |
| Desktop simulator build | PASS; [log](../docs/rc/host-build.log) |
| Full-app Monumatic scenario | PASS: 21 assertions covering FX controls/navigation, saved settings and four sends after reboot, audio limits and panic silence; [log](../docs/rc/integration.log) |

Firmware SHA-256:

```text
07a411691da750b4b3b20df61bba6b8fecc008a640ac0e487a458039c9e46ac9
```

These are build, package and host checks. Physical FM-1 worst-case CPU, dropouts, timing and listening have not been tested for this RC. Existing historical validation records describe their named checkpoints, not additional RC hardware testing.

## Source and notices

The full source is viewable under [`source/`](./), including the platform/BSP, bundled packaging resources, fonts, editor and tests. [BUILDING-RC.md](BUILDING-RC.md) is the current rebuild guide. The [previous README](docs/history/README-before-RC.md) and numbered notes remain available as history.

Missing full upstream notices have been restored without changing the existing component license terms: AMB-1 and fm1-base `LICENSE` from their respective [AMB-1](https://api.github.com/repos/charlesvestal/fm1-amb/contents/LICENSE) and [fm1-base](https://api.github.com/repos/charlesvestal/fm1-base/contents/LICENSE) repositories; felucca-bsp `LICENSE` and `NOTICE` from [felucca-bsp](https://github.com/hugelton/felucca-bsp); the [Barlow OFL notice](https://api.github.com/repos/charlesvestal/fm1-base/contents/assets/fonts/Barlow-OFL.txt); and [stmlib's license](https://github.com/pichenettes/stmlib/blob/master/LICENSE). See [LICENSING.md](LICENSING.md) and the file-level notices for component attribution.
