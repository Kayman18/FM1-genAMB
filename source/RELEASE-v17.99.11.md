# genAMB v17.99.11 — pending UI changes

Built from v17.99.10. Package identity FM-1_7179911.

## Included

- Reverb Sends and Reverb Mix: double-width 2×2 control grid. Existing Reverb Settings 2×2 layout retained.
- PAD 1 and PAD 2 main, Body and SEQ pages: double-width 2×2 control grids. Empty positions remain empty when fewer than four controls exist.
- M1–M6: M7–M11-style waveform panel and double-width Destination/Shape boxes. Fixed pad destination and smooth-random shape remain fixed; no new modulation DSP or routing was added. Row 1 K1 Source and K4 Depth; row 2 K3 Speed. SELECT chooses the row.
- Rings main, Tone and Exciter: double-width 2×2 control grids.
- Rings Delay: 2×2 Time, Feedback, Mix and Repeats. Delay 2: 2×2 Diffusion, Motion, Rev Send and an empty fourth cell.
- Rings UNIT-D Options: 2×2 Tolerance, Range, Rate and Polyphony; existing graph retained.
- Wide-grid pages keep all four physical knobs active without requiring SELECT to change rows. Blank positions do nothing. Existing shared-panel text-spacing fixes retained.

Rings button cycle: Rings → Tone → Exciter → Delay → Delay 2 → Rings Sequencer → mode-specific Options (Proteus or UNIT-D) → Rings. Original and Hallucigenia return directly to Rings after the Sequencer page. The dedicated Rings Sequencer mode/parameter layout itself is unchanged; “SEQ Rings” in the request was interpreted as the Rings instrument pages reached by the SEQ hardware button, as in the queued confirmation.

## Retained behavior

All 13 Rate choices from v17.99.10 remain. FM maximum is six voices, default four. Saved project format remains 22 and no parameter IDs or DSP were changed for this release. FM filter remains on CPU1. The previously reported high-octave FM loudness issue is still unresolved and this release does not change its sound.

## Validation

- Production host UI tests passed for all 15 wide pages, including edits and knob mappings regardless of prior row state.
- M1–M6 fixed fields and Depth/Speed controls verified; existing M7–M11 controls verified.
- All six Rate controls tested for ordered stepping, bounds, labels and all 13 saved IDs across all pieces.
- Native navigation tests passed through the second Delay page and UNIT-D Options, plus PAD 2 and Reverb cycles. Screens for all changed page types inspected.
- Project/filter migration suite passed; native FM sequencer tests (including fractional rates, gates and stop) and 102 worker-policy checks passed with sanitizers.
- JieLi PI32v2 hardware build passed memory, RAM-resident code, inline 64-bit multiply/load, soft-double and register-access checks.
- Package CRCs, loader, decrypted app and padding verified.
- No physical FM-1 boot/listening test performed here. Host performance is not hardware CPU-load evidence.

## Package and memory

Firmware 610108 bytes; application 309912 bytes. Normal RAM 96256/98304 (2048 free), pool 269684/344064 (74380 free).

SHA256: `1012ebb8eca8cae993450012ad216317d848bb131faea7bde1b9cf7f3278db26`

Compiler and SDK resource files remain external. This build uses the same DejaVu Sans Condensed substitutions as the baseline. See BUILDING-v17.99.11.md.
