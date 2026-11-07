> **Historical README, superseded by genAMB RC.** The content below is preserved from the uploaded TEST3b source. Its version, build-status and pending-work statements describe earlier checkpoints. Relative links in the archived text refer to the original `source/` directory. Use the [current README](../../README.md), [RC build instructions](../../BUILDING-RC.md) and [RC release notes](../../RELEASE-RC.md).

---

# genAMB DUSK Monumatic Reverb TEST2

Corrected DUSK TEST4 base with the Monumatic replacement. Build with
`sh build-test.sh`. See [MONUMATIC-TEST2-NOTES.md](MONUMATIC-TEST2-NOTES.md)
for controls, validation and hardware test steps. Original documentation below
is retained as history.

---

# genAMB 1.0 RC2

Current release: upstream Hallucigenia sequencing-core port for Rings and FM. See [release notes](RELEASE-1.0-RC2.md), [build instructions](BUILDING-1.0-RC2.md) and [upstream adaptation notes](app/vendor/hallucigenia/UPSTREAM.md).

The documentation below includes historical project information.

# Current release: genAMB v17.99.8 — Classic FM Filters

See [release notes](RELEASE-v17.99.8-Notes.md) and
[rebuild instructions](BUILDING-v17.99.8.md). Rebuilt from the v17.99.7
source checkpoint. Project format 22; old filter selections migrate.
The original project documentation and historical release notes follow.

# genAMB v17.99.7 for M-VAVE FM-1 — current release

The user-requested cosmetic update from **v17.99.6** is **built and package-validated** using the JieLi PI32v2 cross-toolchain. All ten approved page-layout changes are implemented. The prior historical work-in-progress queue, kept in `history/`, is **not** an active pending build.

See [`RELEASE-v17.99.7-Notes.md`](RELEASE-v17.99.7-Notes.md), [`BUILDING-v17.99.7.md`](BUILDING-v17.99.7.md), and [`NEXT_BUILD_QUEUE.json`](NEXT_BUILD_QUEUE.json).

Sequencer, synth, filter, MIDI, saved-project format (21), and audio DSP source are unchanged from v17.99.6. Device playback/CPU timing remains for physical FM-1 testing.

---

## Historical earlier README (superseded)

# AMB-1 for the M-VAVE FM-1

## Pending next build — saved, not built

This working source includes the approved changes beyond v17.99.2:

- Rings keeps **Modal, Sympathetic and String**; the other four models and their
  unused effects storage are removed. Old saved model IDs have defined fallbacks.
- Standalone FM supports **1–6 voices**, keeping the default selection at four.
- PAD1, PAD2, Rings and FM octave controls add **−3 and −4**.
- The shared canvas fits the existing 240×132 maximum draw band, saving memory.

The user has asked to save these changes for the next build. **Do not build or
package a new release until they explicitly request it.** No new device image
or full host application has been built. Native regression results, resource
projections and compatibility details are in [PENDING_CHANGES.txt](PENDING_CHANGES.txt)
and `tests/next-build/`. `NEXT_BUILD_QUEUE.json` is the active queue.

The completed v17.99.2 release described below is the historical baseline.

## genAMB v17.99.2 — FM16 release

The completed device firmware includes all approved changes:

- Sixteen FM presets: Koto, Harp, Brt Harp, Drop, Mallet, Bell, Rin, Bamboo,
  Furin, Marimba, Celeste, Kalimba, Xylophone, Glocken, Warm Bass and Reso Bass.
- Independent Rings/FM Evolve rates: 1/4x, 1/2x, 1x, 2x and 4x.
- The approved FM-style main pages for Pad 1, Pad 2 and Rings.
- Mixer header above a full-height Pad/Rings/Pad 2/FM control panel.

Four FM voices, the existing gain, v17.99.1 C2 fixes and memory guards remain.
Existing project format18 saves load forward, with old rate values preserved.
Older firmware cannot load saves using the additional preset indices or rates.

Read [V17_99_2_FM16_NOTES.txt](V17_99_2_FM16_NOTES.txt) for the complete bank,
controls, compatibility, measured resources, validation and build instructions.
Native final screenshots are in docs/release-17.99.2/. Reproducible checks and
small validation logs are in tests/fm16/ and tests/release-17.99.2/.

Build with `sh build.sh --release 17.99.2`, then run
`python3 tests/fm/validate_package.py`. The output is
`app/build/amb-17.99.2.fwsc`, release identity FM-1_7179902.
The external toolchain and SDK are described in BUILDING.md.

The work released in v17.99.2 is recorded in
`history/NEXT_BUILD_QUEUE-completed-17.99.2.json`. Earlier candidate and feasibility
notes are historical records; the pending next-build section above is current.

## Base release history

## genAMB v17.99.1 FM Fix

This update addresses the C2 worker fault reported while changing FM settings
and puts **FM in the fourth Mixer strip**, with **Knob 4 controlling FM level**.
The FM meter now shows quiet signals down to -60 dBFS. Master reverb Mix is on
the third FX page, after Reverb and Sends.

The FM sample loops use exact 32-bit arithmetic and run from the existing
executable RAM reservation. The worker hang limit is now two audio buffers
(11.610 ms), with separate Timeout, Check and Stack fault indications.
Audio-overrun reporting and mailbox/stack guards remain active. Device build,
worker fault-policy and rapid live-change tests pass; physical worst-case
timing and the reported C2 recurrence still need an on-device retest.

The four-voice, six-operator FM layer retains its nine Zen-1 patches,
independent Original and Evolve sequencers, and separate reverb send.
All seven Rings models remain available. Press **SEL** for the FM pages.
FM starts at Level 0 in factory pieces and projects migrated from v17.98.
Existing v17.99 projects keep their settings; the project format remains 18.

Read [V17_99_1_FM_FIX_NOTES.txt](V17_99_1_FM_FIX_NOTES.txt) for complete
controls, changes, validation, compatibility and build instructions. The
previous [V17_99_FM_NOTES.txt](V17_99_FM_NOTES.txt) is retained as history.
The inherited online links and older manual below describe upstream AMB-1.

AMB-1 turns the M-VAVE FM-1 into a generative ambient machine, inspired by Brian Eno's
*Music for Airports*. It plays itself from the moment it's switched on: breathing voices, soft
bells on tape loops of unequal length, a gliding bass and quiet Euclidean percussion, all through
a tape echo, a long tape loop and a plate reverb. You don't perform it, you tend it: pick a piece
and a scale, take notes out, put them back, and let it grow.

![AMB-1's screen: the garden](docs/img/screen-garden.png) ![Voices](docs/img/screen-voices.png) ![Frozen](docs/img/screen-frozen.png)

- **Try it in your browser** (no FM-1 needed): <https://charlesvestal.github.io/fm1-amb/emu/>
- **Install it** (Chrome or Edge, with the FM-1 plugged in): <https://charlesvestal.github.io/fm1-amb/install/>
- **Read the manual**: <https://charlesvestal.github.io/fm1-amb/manual/> (source: [docs/MANUAL.md](docs/MANUAL.md))
- **Download the firmware file**: [releases](https://github.com/charlesvestal/fm1-amb/releases)

Installing it is at your own risk, though it's hard to get stuck: the web installer can put
M-VAVE's own firmware back, and an FM-1 that fails to start twice comes up in a safe mode the
installer can reach.

## In short

| | |
|---|---|
| Keys | The scale: each key adds its note or takes it out |
| PLAY · REC · EDIT | Grow / rest · freeze · the next chord |
| HOME · ENV · ARP · LFO · SEQ · FX · GLO | Garden · Pad 1 · Pad 2 · Mod · Rings · Reverb Settings · Mixer (press GLO again for Setup) |
| SELECT | The previous or next page; with HOME held, the tempo |
| SEL button | FM pages: instrument, motion, sequencer and Evolve controls |
| ALGORITHM · PRESETS | The mode · the piece (eight of them) |
| OCT− / OCT+ | Pad 1, Pad 2, Rings or FM octave, following the current page |
| SAVE | Saves; held, puts the piece back to its recipe |

Hold any button for a second and a card says what it does.

AMB-1 also tells the [FM-1 Screen Reader](https://charlesvestal.github.io/fm1-reader/) what its screen
shows, over USB: the page and its knobs, a knob's value as it turns, the piece, the chord as it moves,
and what each button did.

The [manual](https://charlesvestal.github.io/fm1-amb/manual/) has the rest.

## Building and testing

AMB-1 is an app on [fm1-base](https://github.com/charlesvestal/fm1-base) (`base/`, a git submodule); clone with
`--recursive`. The toolchain and prerequisites are in [BUILDING.md](BUILDING.md):

```
sh tests/run_tests.sh                                  # the instrument, the app in a simulator, the web builds
./build.sh                                             # app/build/amb.fwsc
python3 base/tools/fm1_install.py app/build/amb.fwsc   # install it over USB
```

`app/build/host/amb_host SCRIPT OUTDIR` runs the whole app on a computer from a script
(`tests/scenarios/*.amb`), with audio and screenshots coming out. `./build.sh --web` builds the same code for the
browser (the emulator); `web/app/build.sh` builds the engine alone into a web app for phone and desktop.

## Credits

The idea comes from Brian Eno's generative music: *Music for Airports*' tape loops of unequal
length, and the echo chain of *Discreet Music*. The reverb is Jon Dattorro's plate ("Effect Design,
Part 1", JAES 1997), as in [FoMni-1](https://github.com/charlesvestal/fm1-fomni); the Euclidean
rhythms are Godfried Toussaint's reading of Bjorklund's algorithm. AMB-1 is built on
[fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp),
from [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton Instruments). The idea of running
FM-1 firmware in the browser comes from [Groove OS](https://www.groove-os.com/). MPL-2.0 (0.9 and earlier:
GPL-3.0-only); see [LICENSING.md](LICENSING.md).

M-VAVE and FM-1 are trademarks of their owners. AMB-1 is not affiliated with M-VAVE, Brian Eno or
Hügelton Instruments.


## Pad 2 Lite (v17.4)
The former Drone layer remains a second, independent four-voice Pad engine, with its own envelopes, oscillator phases, random modulation, voice leading and chord progression. Its breath/modulation/chord clocks run at half the speed of Pad 1. To recover FM-1 real-time CPU headroom, Pad 2 uses a lighter full-rate voice path: soft oscillators, a single stereo bus low-pass and phase-safe mid/side width in place of the second modulated chorus delay and per-voice resonant filters. Pad 1, Rings and the v17 reverb path are unchanged.

## v17.6 UI + Pad 2 colour update
Button/parameter confirmations now appear on a single header line in one consistent font (for example `RINGS OCTAVE +1`). Pad 2 keeps the Lite CPU path but now has its own complementary Colour choices: `Glass` and `Strings`, instead of duplicating Pad 1's `Rhodes` and `Vintage`.

## v17.7 local build changes

This build adds a lightweight Rings-only mini delay (20-800 ms, Feedback, Mix) on a fourth Rings page. Mix defaults to 0 for compatibility. It also doubles the Pad 1/Pad 2 breath-fall duration and doubles their release/handoff fades, while leaving attacks unchanged. See `V17_7_RINGS_DELAY_LONGER_PAD_FALL_VALIDATION.txt` in the packaged release for details.

## v17.9 button map + extended Rings delay

The custom page buttons are now: **ENV = Pad 1**, **ARP = Pad 2**, **SEQ = Rings**, **FX = Reverb Settings**, and **GLO = Mixer**. GLO pressed again opens Setup so that page remains accessible; all other hardware buttons keep their existing functions. The main panel's third row is now a plain numeric mix summary, while the only animated VU meters are in bottom panel 4 on the Mixer page beside Knobs 1-4.

The Rings-only mini delay now reaches **1.5 seconds**. To stay inside the FM-1 RAM budget it stores a filtered 11.025 kHz mono feedback line in 16-bit memory and interpolates the full-rate output. Delay settings through the lower 80% of the Time knob retain the previous 20-644 ms mapping; the top 20% extends from 644 ms to 1.5 s.

## v17.12 clean Rings delay

The Rings-only delay was upgraded to filtered **22.05 kHz / 16-bit** storage with 2:1 downsampling and interpolated reads while keeping the 20-1500 ms range. Its previous internal write-gain error was corrected, and the buffer is split between normal RAM and `.pool` memory so the higher-bandwidth delay still fits the FM-1.

## v17.17 prominent additive delay mix

The Rings delay Mix control now behaves like an effect-send return rather than a dry/wet crossfade. Dry Rings remains at full level and delayed audio is added on top. The Mix curve is square-root shaped and cached at control rate: Mix 50 gives about 0.95x wet return, while Mix 100 gives 1.35x wet return. Feedback and Repeats are unchanged. This makes the delay much more audible without adding another buffer or meaningful audio-thread CPU cost.
