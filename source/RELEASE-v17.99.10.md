# genAMB v17.99.10 — wider controls and fractional sequencer rates

Based on the device-confirmed v17.99.9. Identity FM-1_7179910.

## Included changes

- M7–M11: Destination spans two panels in the upper row, Shape spans two in the lower row, and shape names use the matching value font without the old nine-character truncation.
- Mod controls: upper row K1 Source, K2 Destination, K4 Depth (K3 unused); lower row K1 Shape, K3 Speed (K2/K4 unused). SELECT switches rows. M10 and M11 retain their fixed FM Cutoff/FM Resonance destinations.
- Reverb Settings: Space, Decay, Damping and Motion are each two panels wide in a 2×2 grid. K1–K4 always control these four settings; SELECT cannot disable a row. Sends and Return pages retain their existing layouts.
- Shared status panels have extra vertical clearance for their bottom instruction text, including the M7–M11 waveform panel.
- All six Rate controls, on Rings and FM Hallucigenia, Proteus and UNIT-D, now offer: **÷4, ÷3.5, ÷3, ÷2.5, ÷2, ÷1.5, ×1, ×1.5, ×2, ×2.5, ×3, ×3.5, ×4**. Display uses 1/…x for division. Default stays ×1.
- Fractional steps are scheduled between sixteenth ticks at audio-sample boundaries. External clock uses the measured sixteenth interval to interpolate between incoming ticks; abrupt clock changes/jitter can affect these estimates.

## Compatibility and retained behavior

Original five Rate IDs retain their meanings; additional choices use appended IDs, with UI ordering handled separately. Existing saved projects keep their speeds and format 22. All 13 IDs were save/load tested across the active settings and all eight pieces. If returning to older firmware, newly added Rate values are outside its accepted range and can cause project reset; retain the older project before downgrade.

FM remains capped at six voices, default four. The FM filter remains on CPU1 pending hardware timing evidence. This build does not change the high-octave FM sound or level; that reported issue is still unresolved.

## Validation

- Host tests: musical-order stepping and endpoint bounds on all six Rate controls; labels and all saved Rate IDs; M7–M11 Shape/Speed/Depth bindings; all four Reverb controls in either prior row state.
- Production FM sequencer: all 13 rates through Proteus and UNIT-D, internal/external clock paths, timestamps within half a sample of the expected position at the tested constant tempo, and stop cancellation. Original five-rate timing, gate, MIDI and loop tests passed with address/undefined-behavior sanitizers.
- FM adapter: 84 checks passed in normal and sanitizer builds; upstream synthesis fingerprint unchanged.
- Worker policy: 102 checks passed. UNIT-D native DSP suite passed. Production project-loader/filter migration suite passed.
- Native screen captures inspected for the wider controls and status-panel spacing.
- JieLi PI32v2 hardware build passed RAM-resident code, inline 64-bit multiply/load, soft-double and register-access checks. Package CRCs, loader, decrypted app and padding verified.
- No physical FM-1 boot, listening or CPU timing test performed here. Host performance is not FM-1 CPU-load evidence.

## Package

610108 bytes; application 309448 bytes. Normal RAM 96256 / 98304 (2048 free). Pool 269684 / 344064 (74380 free).

SHA256: `3c93a87590aa5037e2ccdb4a7e5880666155540af4590cd112a311fa36e44d6f`

See BUILDING-v17.99.10.md in the complete source. Compiler, SDK resource files and font binaries remain external dependencies; font substitution matches v17.99.9.
