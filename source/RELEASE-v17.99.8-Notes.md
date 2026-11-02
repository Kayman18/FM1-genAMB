# genAMB v17.99.8 — Classic FM Filters

**Rebuilt and package-verified for the M-VAVE FM-1 / JieLi PI32v2.**
This release was rebuilt from the preserved v17.99.7 complete source using the
uploaded `jieli-linux-toolchains-20260730.1` compiler. The previous completion
message had no recoverable v17.99.8 release artifact; this archive and firmware
are the actual outputs of the rebuild. These results supersede that message's
unverified resource figures.

## Firmware

- File: `genAMB-v17.99.8-Classic-FM-Filters.fwsc`
- Identity: `FM-1_7179908`
- Package length: **610,108 bytes**
- Application: **308,184 bytes**
- SHA-256: `75c2c5aeb71bf31c0ed7d21811dc80f77cb5418c1fbda395d024cf0fff1299cc`
- Normal RAM: **96,272 / 98,304 bytes**; **2,032 bytes free**.
- Audio pool: **270,932 / 344,064 bytes**; **73,132 bytes free**.
- Project storage: **format 22**.

## Replacement filter bank

The previous Standard/Diode/Korg35/Moog/Oberheim bank is replaced by six choices.
These are distinct original DSP implementations/variants, not renamed presets,
not a port of commercial synthesizer firmware, and not measured or
component-calibrated emulations of the named hardware.

| Model | Implemented processing | Type control |
|---|---|---|
| Bypass | Unfiltered FM sum | Bypass |
| Clean SVF | Linear topology-preserving two-pole state-variable filter | LP / HP / BP / Notch |
| SEM-inspired | Two-pole SVF with a soft input stage, different resonance law and soft voltage limiting | LP / HP / BP / Notch |
| Juno-inspired | Four equal trapezoidal poles, analytically solved global feedback, partial resonance gain compensation | LP |
| Polysix-inspired | Four slightly spread, individually saturated poles; nonlinear global feedback solved with three bounded Newton iterations | LP |
| CS-80-inspired | Two-pole high-pass followed by two-pole low-pass, with separate states and a soft interstage drive | HP+LP |

The CS-80-style section shares one Cutoff knob: its high-pass cutoff is linked
**two octaves below** the low-pass cutoff. It does not expose two independent
cutoffs like the original instrument. SEM and Clean necessarily share the
state-variable topology family; their nonlinear processing and resonance laws
are different. These qualifications matter when evaluating authenticity.

The filter processes the **shared mono FM sum**, before that sum is added to
both stereo channels and the reverb send. It is not duplicated per FM voice.
Only the active model runs. Cutoff/resonance/mix are smoothed. A model/type
change fades the filtered contribution through dry before resetting the new
state; this avoids an abrupt full-wet state reset. Settled Bypass and settled
zero Mix pass the dry sample exactly.

## Controls and retained features

FM Filter retains the approved 2×4 screen:
- Row 1: Type, Cutoff, Resonance, Mix.
- Row 2: Model, followed by three empty reserved cells.
- SELECT selects the row; K1–K4 edit that row.

The Model cell now invalidates/redraws when its value changes (a stale-label
issue in the old draw-cache key was corrected). Clean SVF is split over two
lines to fit; the Resonance caption is shortened to Reson. in this narrow cell.
The response graph remains a **schematic illustration**, not a measured
frequency-response plot of the nonlinear algorithms.

Ten FM voices, all 16 FM patches, M10 cutoff and M11 resonance modulation,
M7–M11 shapes/speeds/depths, all four Rings/FM sequencers, the three retained
Rings models, pads, parallel reverb, delay, C2 protections and the v17.99.7
cosmetic layouts are retained. The FM patch data and FM core are unchanged.

## Saved-project migration and rollback

Formats 20 and 21 migrate to format 22, preserving other parameters and banks.
The meaning of the filter IDs changed, so the old selections are translated:

| Old selection | New selection |
|---|---|
| Standard with Type Bypass | Bypass |
| Standard LP/HP/BP | Clean SVF with the same response |
| Diode Ladder | Polysix-inspired LP |
| Korg35 HP | SEM-inspired HP |
| Korg35 LP | SEM-inspired LP |
| Moog24 | Juno-inspired LP |
| Moog12 | SEM-inspired LP |
| Oberheim | SEM-inspired with the same response |

These mappings preserve approximate response categories, **not the old tone**.
Cutoff, resonance and mix values are retained. Formats 19 and earlier keep the
existing forward migrations and start the new filter in Bypass.

**After SAVE or autosave writes format 22, v17.99.7 cannot read that save.**
Keep your known-good v17.99.7 firmware and a settings backup before testing.
Firmware rollback and saved-project compatibility are separate concerns.

## Completed software validation

- Real PI32v2 release compilation and RAM/pool/image checks passed.
- Register policy, forbidden 64-bit multiply/load-pattern and soft-double
  checks passed. RAM-code inspection: 929 instructions, no calls.
- Firmware length, identity, CRCs, loader equality, decrypted application
  equality and erased application-slot padding passed.
- 864 model/type/cutoff/resonance combinations passed AddressSanitizer and
  UndefinedBehaviorSanitizer with finite output and finite internal states.
- Modulation sweeps, silence, topology switching and settled exact bypass/mix
  checks passed. LP/HP/BP/notch and dual HP/LP response checks passed.
- All five active models produced distinct output on identical test input.
- Cutoff coefficient error remained below 0.15% in the tested 0–100 knob sweep.
- Production project loader passed all 28 old model/type translations across
  the active project and all eight pieces, preserving unrelated project bytes.
  Format-20 migrations, format-22 save/reload and invalid-ID checks passed.
- Native UI navigation and SAVE/reboot passed; six native filter screenshots
  are in the validation archive.
- FM adapter: 41 checks passed, including sanitizer run and unchanged upstream
  core fingerprint. FM sequencer checks passed, including 1–10 voice MIDI
  allocation. Worker policy: 102 checks passed under sanitizers.
- UNIT-D production tests and both-layer application/SAVE/reboot scenario
  passed. This does not constitute exhaustive testing of every combination.

No physical FM-1 timing or listening test has been performed. In particular,
the nonlinear Polysix-style filter costs more CPU than the linear filter.
Begin at four or six FM voices; increase to ten only while checking audio
stability and worker timing. 2C means the worker started; C2! is a fault.

## Fonts and rebuilding

All six generated font-role bitmap arrays were found **byte-identical** in the
released v17.99.7 firmware. This rebuild therefore preserves that release's
condensed-substitute lettering, rather than silently switching fonts.
No font binaries, proprietary compiler or SDK inputs are redistributed in the
source ZIP. Follow `BUILDING-v17.99.8.md` for external dependencies.
