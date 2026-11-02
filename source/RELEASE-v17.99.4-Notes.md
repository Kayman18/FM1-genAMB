# genAMB v17.99.4 — Proteus + Hallucigenia — FM-1 Test Firmware

**Status:** Cross-compiled for actual JieLi PI32v2 / MVave FM-1; package, memory, processor instruction and focused host/sanitizer checks PASS. Not yet tested on physical FM-1. Preserve v17.99.3 for rollback before trying this build.

## What changed (from v17.99.3)

- Third sequencing mode **Proteus** added to both Rings and the six-operator FM instrument.
- **Original / Hallucigenia / Proteus** selectable independently for Rings and FM. Hallucigenia is the renamed Evolve mode; the underlying existing 16-pattern Hallucigenia audio and note engine is unchanged.
- Both sequencer pages use the approved **Mode + 2 rows × 4 controls** layout and their own existing Rings-magenta / FM-orange colours. The Mode selection uses Knob 1 *only while Mode is highlighted*; SELECT moves between Mode, Row 1 and Row 2. Root Note changes with SELECT *only on the Home page*; the existing HOME+SELECT Tempo shortcut remains.
- Proteus per layer: 32-step melody generation with source-like 5:5:5:10 Repeat/Up/Down/New weighting, shuffled density rests with at least one step, probabilistic octave moves and single-step mutations, Poisson-CDF **Patience** regeneration, and the original three Lock behaviors (Auto, No Mutate, Locked).
- **Proteus always follows the active chord supplied by genAMB's Original chord progression**, mapping its stored melodic chord-degree ranks to that chord at playback rather than rewriting the pattern; no selectable scale-follow in Proteus.
- Both Proteus layers have separate melody state, parameters, random generator and rate (¼×, ½×, 1×, 2× and 4×).
- An optional **Proteus Options** page (RINGS or SEL once more while Proteus is selected) provides Range (Fixed / ±1 / ±2), New Melody, Octave, and Octave Drift. No other synthesizer voices or buffers were added.
- Older mode choices continue to switch on the next bar while sounding tails continue. Existing Original and Hallucigenia patterns are preserved.

## Controls (both Rings and FM sequencer pages)

**SELECT encoder** highlights: MODE → ROW 1 → ROW 2 (cycles with further turns).

- **Mode:** Knob 1 selects Original / Hallucigenia / Proteus. Other three knobs do not change anything while Mode is selected.
- **Hallucigenia Row 1:** Length / Mutation / Density / Rate.
- **Hallucigenia Row 2:** Range / Follow / Pattern / New.
- **Proteus Row 1:** Length / Density / Patience / Rate.
- **Proteus Row 2:** Gate / Oct Prob / Mutate / Lock.
- **Original Row 1:** Octave / Oct Drift / Chance / Burst.
- **Original Row 2:** Density / Level / Model (or Patch) / Voices.

The physical RINGS button cycles the Rings-related pages and then the Rings SEQ page; SEL cycles FM / FM Motion / FM SEQ. While Proteus is selected on a sequencer page, pressing the respective page button again opens its Proteus Options page, then cycles back.

**Gate mapping:** FM uses Proteus Gate as note-on duration until release; Rings is a naturally decaying struck resonator, so its Gate control scales excitation strength rather than controlling a synthesizer sustain gate. This is a hardware adaptation of the VCV behavior.

## Provenance / licensing

Proteus algorithm is based on the source code by Seaside Modular: https://github.com/abluenautilus/SeasideModularVCV/blob/master/src/Proteus.cpp (MIT license). Relevant mechanisms are adapted to fixed-size FM-1 state and the user's global chord following. The VCV-specific UI, external CV, JSON, and ProteusX expander are not ported. The existing genAMB FM/other licensing remains unchanged; see LICENSING.md. No unlicensed Gina's ARP source is included.

## Memory and device build

- Target compiler: JieLi PI32v2, Linux toolchain **20260730.1**.
- Firmware package: **610,108 bytes**; identity `FM-1_7179904`.
- Application: **275,112 bytes**.
- Normal RAM: **96,512 / 98,304 bytes** (free **1,792 bytes**).
- Audio memory pool: **262,304 / 344,064 bytes** (free **81,760 bytes**).
- Validation: instruction restrictions, normal RAM and pool limits, MMIO/register access, firmware header/table/payload CRCs, loader and decrypted application bytes, and slot padding PASS.
- No additional synthesis voices/audio buffers; six FM voices maximum and all 16 FM patches remain included.

## Focused tests

- 32-step Proteus note production on both layers, independent switching on bar boundaries, a confirmed Original-chord change while both played, zero off-chord generated notes in the tested sequences, and locked-pattern stability: PASS.
- Format-19 project save/reboot (both melodies and new settings): PASS. Migration from format 18: PASS (default Proteus state, original parameter values retained).
- Normal and AddressSanitizer/UndefinedBehaviorSanitizer host runs of those tests: PASS.
- Existing FM port: **41 tests PASS** including sanitizer, zero failed.
- Existing C2-worker policy: **102 checks PASS** normal and sanitized.
- Existing FM sequencing isolated harness: harmonic mapping **138,240 cases**, original loops, all five rates, internal/external clock gate timing, polyphony 1–6: PASS normal and sanitized.
- **Unmodified Original and Hallucigenia playback:** 10-second host WAV from v17.99.3 vs v17.99.4 were byte-identical in each of two tests.
- UI: produced source-rendered 240×240 screenshots for both instruments and all three SELECT positions.
- Physical FM-1 realtime/CPU1 timing under six FM voices and rapidly changing settings: **not yet verified**. Please monitor the existing Worker diagnostic field for C2 faults and test at 1×/four FM voices first before 4×/six voices.

## Project format / rollback

Project format is now **19**. Existing v17.99.3 format-18 projects migrate to format 19 on load. **After you SAVE under v17.99.4, v17.99.3 cannot read those updated saves** and will fall back to defaults. Back up your project settings before flashing or saving. The previous firmware can still be flashed as a rollback, but newer saved projects are not backward-readable.

## Build dependencies

`JIELI_TOOLCHAIN` should point at the extracted JieLi PI32v2 Linux toolchain root, and `AC79_SDK` at the SDK boot resources. Build with `sh build.sh --release 17.99.4`. Font source files are not redistributed with this source archive; the build needs independently obtained Barlow Semi Condensed fonts at the paths specified in the existing build instructions. The firmware ZIP includes compiled glyph tables in the binary only.
