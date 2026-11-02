# genAMB v17.99.7 — Cosmetic Pages Release

**Target:** M-VAVE FM-1 (JieLi PI32v2)  
**Release identity:** `FM-1_7179907`  
**Based on:** v17.99.6 UNIT-D + horizontal segmented mixer  
**Project format:** **21, unchanged** from v17.99.6; no migration or new save fields  
**Hardware status:** real target compilation and package verification passed; **physical FM-1 playback and timing have not been tested**.

## All 10 approved changes

1. **Rings Tone:** Move Damping and Position from Exciter to Tone; Tone now has Structure/Brightness/Damping/Position on its first row.
2. **Rings Delay:** Merge Delay + Delay Ambient into one 2x4 page: Time/Feedback/Mix/Repeats; Diffusion/Motion/Rev Send/empty. Remove separate Delay Ambient from button cycle.
3. **FM Main:** Move Density, Burst and Reverb Send from FM Motion to FM Main second row; remove FM Motion from SEL page cycle. Row 1 stays Patch/Level/Voices/Gate.
4. **HOME:** Remove all VU strips and use standard 2x4 grid with existing Tempo/Evolve/Motion/Drift settings on row 1.
5. **HOME / GLO Setup:** Move Root out of GLO Setup into first empty HOME row-2 cell. Ordinary SELECT still adjusts Root on HOME; hold HOME+turn SELECT to change HOME row.
6. **FX Reverb / Sends / Reverb Mix:** Remove live mixer/VU section; use 2x4 visible boxes with unused slots blank.
7. **ENV PAD1:** Remove VU on PAD1, Body and PAD1 Seq screens and use 2x4 setting panels with unused slots blank.
8. **ARP PAD2:** Remove VU on PAD2, Body and PAD2 Seq screens and use 2x4 setting panels with unused slots blank.
9. **SEQ Rings:** Use 2x4 grid on every reachable Rings page without moving controls to another page except the specifically approved relocations; preserve mode selector zone and existing UNIT-D options page.
10. **GLO Setup:** Keep only top page header and 2x4 setting boxes, with Tune/MIDI out/Lights and five blank slots.

## How the row controls work

On any standard 2x4 page, turn **SELECT** to highlight Row 1 or Row 2. Both rows always remain visible; physical **Knobs 1–4** edit only the active row. Empty cells are intentionally shown, are not functional, and do not consume instrument voices.

**HOME exception:** ordinary SELECT always changes **Root Note**, including when HOME Row 2 is visible. Hold the HOME physical button and rotate SELECT to choose the HOME row. On HOME Row 2, Knob 1 also edits Root Note, making it accessible as a visible setting box. This replaces the old HOME+SELECT tempo shortcut; Tempo stays at HOME Row 1 Knob 1.

**Rings Delay / Ambient:** SEQ cycles Rings → Tone → Exciter → Delay (merged) → Rings Seq → relevant sequencer options. On Delay, Row 1 controls Time, Feedback, Mix and Repeats. Row 2 controls Diffusion, Motion and Reverb Send; the final position stays empty.

**FM Main:** SEL cycles FM Main → FM Filter → FM Seq → optional sequencer options. The removed FM Motion parameters are on FM Main Row 2 in the order Density / Burst / Rev Send / blank.

**Reverb:** FX cycles Reverb, Sends and Reverb Mix; all show the standard 2x4 cells, with blank reserved slots. GLO Mixer's horizontal segmented channel meters are **retained** unchanged. GLO Setup now consists of the header and the 8-cell settings grid only.

## Preserved sound and system behavior

- Ten-voice selectable FM, the 16 FM presets and seven FM filter models are retained.
- Rings Modal, Sympathetic and String, both pads, delay and parallel reverb, M1–M11 modulation, Original, Hallucigenia, Proteus and UNIT-D are retained.
- Original chord tracking, live sequencing, C2 worker watchdog and dual-core rendering are unchanged.
- The same numbered parameter IDs, ranges and saved-project format 21 remain; saved v17.99.6 projects continue to load normally.
- UI changes do not alter the DSP processing implementation.

## Test summary

- **Device build:** PI32v2 target linker and memory/instruction checks pass; image size **306,440 bytes**.
- **Memory:** normal RAM **96,272 / 98,304** bytes, audio pool **270,932 / 344,064** bytes.
- **Firmware package:** 610,108 bytes, identity `FM-1_7179907`, CRCs, loader, decrypted application and padding verified.
- **UI and controls:** all ten relocations and page-cycle shortcuts tested in the simulator, and targeted SAVE/reboot test passed without project format change.
- **Sequencers:** UNIT-D integrated host scenario passed with both Rings and FM after accounting for shorter button cycles. UNIT-D determinism, lock and chord tests passed.
- **Regression:** 41 FM adapter tests, sequencer tests including ten voices, 102 C2 worker checks, 700 filter model parameter combinations passed.

The tests above use the host or cross-compiler, not physical FM-1 audio deadline measurements.

## Font reproducibility note

The current delivery environment did not contain the original three **Barlow SemiCondensed** TTF inputs (the archived source package, as in previous genAMB releases, does not bundle third-party TTF assets). For this specific release's generated C glyph tables, the build used **system DejaVu Sans Condensed regular/bold** mapped to the expected generator inputs. The layout, colours, controls and dimensions are native genAMB; letter shapes/spacing can differ slightly from v17.99.6. Source consumers who want the original lettering can obtain the appropriately licensed Barlow SemiCondensed fonts and regenerate `felucca_font.h`, then rebuild. No font files are bundled or redistributed in this source ZIP.

## Device test recommendations

Preserve the previous known-good v17.99.6 firmware. After flashing v17.99.7, confirm 2C, check HOME root and row switching, move through all Rings/FM/FX/GLO pages, test SAVE and restart, and listen to both sequencers under load. C2! indicates a worker fault, not normal dual-core operation.
