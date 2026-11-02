# AMB-1 licensing

## genAMB v17.99 FM test distribution

This combined firmware adds the Zen-1 six-operator FM engine and its nine original
ambient patches, pinned to `aceyprime/zen-1` commit
`ec9295233ea64ddac224b58b4d07216706c1b8ee`. The combined executable is distributed
under GPL-3.0-only, with complete corresponding source supplied alongside it.
The original notices and licenses of all components are retained: AMB-1 and
fm1-base files are MPL-2.0, Mutable Instruments Rings is MIT, the msfa FM core is
Apache-2.0, and the imported Zen-1 tables, patch codec, patches and generators are
GPL-3.0-only. See `app/vendor/zen_fm/` for the imported license texts, provenance,
generators and changes. The FM adapter is GPL-3.0-only. No Yamaha factory bank
or commercial patch data is included.

The following describes the original AMB-1 components and remains applicable
to those files.

AMB-1 is free software under the **Mozilla Public License 2.0** (`MPL-2.0`, full text in `LICENSE`), file by file.
If you change one of its files and share the result, share that file's source under the MPL. It is built on
[fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp)
(`base/`, a git submodule), both MPL-2.0.

Until 0.9, AMB-1 was GPL-3.0-only, built on Felucca's platform layer. That code is gone from this tree: the platform
is fm1-base's now, and AMB-1's own code (the engine, the UI, the pieces) is relicensed by its author. AMB-1 0.9 and
earlier stay under the GPL.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| The platform: `base/` (boot, USB and USB audio, the TRS jack, storage, the update path, screen, panel, the screen reader, the host simulator and the browser emulator, build and install tools) | [fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp), from [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (@kurogedelic), Hügelton Instruments, under MPL-2.0 with his permission ([felucca-bsp#4](https://github.com/hugelton/felucca-bsp/issues/4)) | MPL-2.0 |
| `app/` (`app.c`, `app.h`, `ui.c`, `project.c`, `host.c`, `dsp/amb.c` with its Dattorro plate from FoMni-1), `web/app/`, `tests/`, `tools/` | AMB-1, Charles Vestal | MPL-2.0 |
| The idea of a browser emulator for FM-1 firmware (fm1-base's emulator) | [Groove OS](https://www.groove-os.com/) by Peter Gombos, whose site runs its firmware in the browser; no code from it | credit only |
| Full Mutable Instruments Rings DSP plus the v15 global reverb adapter in `app/rings_port/`; the global ambience uses the original Mutable Instruments Clouds Griesinger/Dattorro delay topology and MI `FxEngine`, adapted to 44.1 kHz and 32-bit floating delay storage | Mutable Instruments Eurorack source, Emilie Gillet | MIT |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Fonts: Barlow Semi Condensed (The Barlow Project Authors), Terminus (Dimitar Toshkov Zhekov) | SIL OFL 1.1 | `base/assets/fonts/` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. "M-VAVE" and "FM-1" are trademarks of their
respective owners, used here only to say what hardware AMB-1 runs on. AMB-1 is independent firmware; it is not
affiliated with, endorsed by or supported by any of them.

## Radio

AMB-1 never enables the Bluetooth / Wi-Fi radio of the hardware.

## genAMB v17.99.4: Seaside Modular Proteus

The FM-1 Proteus sequencer adaptation in `app/dsp/proteus.c` is based on
Seaside Modular's public MIT-licensed source (original `src/Proteus.cpp`):
https://github.com/abluenautilus/SeasideModularVCV . Its original
melody-generation, density/rest, Poisson patience, octave mutation and locking
concepts are preserved in fixed-size FM-1 state. The original voltage, user
interface and optional expansion wiring are not copied. This version uses
chord-degree mapping so both adapted sequencers always follow the active
Original-progression chord. Source attribution and the upstream MIT notice
are retained; the combined executable remains subject to the other licenses
listed above, including the Zen-1 GPL-3.0-only FM adapter.

## Historical FM post-mix filter model families (v17.99.5–v17.99.7)
The compact model families are inspired by the FAUST filter collection at
https://github.com/SpotlightKid/faustfilters (Christopher Arndt, ISC licence).
Original analog model types and mathematical techniques were documented by Eric Tarr
in the FAUST source (MIT-style STK-4.3 notice). Embedded approximation code
is authored as a new fixed-state C implementation in app/fm_port/fm_filter.c,
not copied verbatim nor bit-identical to the plugin's generated DSP.

## UNIT-D extension (genAMB 17.99.6)

`app/dsp/unitd.c` is a freestanding adaptation of Animatek's GPL-3.0 licensed
UNIT-D (`https://github.com/animatek/UZZ-VCV-RACK/blob/main/src/UnitDistanceSeq.cpp`).
It is distributed under GPL-3.0-or-later with the GPL-3 license text in
`app/vendor/unitd/LICENSE-GPL-3.0.txt`. The combined genAMB firmware is
GPL-3.0; the pre-existing MPL-2.0 and permissive components retain their
original copyright and license notices. This is a hardware-specific adaptation,
not a bit-identical VCV Rack executable.

## Current classic FM filter bank (v17.99.8)

The preceding FAUST-inspired bank is no longer active. The replacement
`app/fm_port/fm_filter.c` and header are original fixed-state ISC-licensed
implementations: a linear SVF, a nonlinear SVF variant, an analytically solved
four-pole cascade, a nonlinear four-pole cascade and a linked HP/LP pair.
SEM, Juno, Polysix and CS-80 identify intended character/topology references,
not copied proprietary code, measured hardware emulations, endorsement or
affiliation. The combined firmware retains the component-license obligations
described above. No third-party code or new font binaries were imported for
this filter replacement.

## genAMB 1.0 RC2: upstream Hallucigenia / Darwinism port

The previous independent Evolve implementation is replaced for both Rings and
FM by an adaptation of Eater of Sheep / Extratone's Hallucigenia source:
https://github.com/EaterOfSheep/Extratone/blob/main/src/Darwinism.cpp .
The core and integration are GPL-3.0-or-later; original upstream source,
license and precise adaptation notes are in `app/vendor/hallucigenia/`.
See `UPSTREAM.md` there for the pinned source blob and hardware differences.
Existing Original sequencing portions retain their MPL-2.0 provenance.
