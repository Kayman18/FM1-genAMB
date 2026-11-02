# genAMB v17.99.9 — FM voice limit and CPU timing

Based on v17.99.8. Firmware identity FM-1_7179909.

- FM maximum is six voices; new/default projects use four. The adapter itself now also initializes to four.
- Existing saved settings of 1–6 are retained. Formerly valid 7–10 settings become six in the active project and all eight pieces; other settings and pattern banks are preserved. Project format remains 22.
- FM filter remains on CPU1 with Rings and FM synthesis. No current FM-1 timing measurements were available to establish CPU0 headroom, so the conditional move is deferred.
- Performance page now adds CPU0 audio work average and peak in microseconds. CPU0 audio work excludes blocking waits for CPU1; it is not whole-core utilization and excludes UI/storage work outside the audio interrupt. CPU1 work and wait are smoothed averages; peaks persist until reset. Host simulator counters remain zero.

## Device measurement

Press GLO until PERFORMANCE appears (Mixer → Setup → Performance). Keep Mode on Dual. Turn knob 2 (Reset) after setting up the sound. Play a demanding passage for at least 60 seconds with both pads, Rings, reverb and FM active. Capture the page at four FM voices, then reset and repeat at six. Compare the same passage with the filter bypassed and with your preferred filter (especially Polysix). Include Audio peak, CPU0 peak, CPU1 work, Wait, Dropouts and Faults. Also exercise normal page navigation. The 256-frame audio deadline at 44.1 kHz is about 5805 microseconds. These readings inform whether a separately tested CPU0 filter route is worthwhile; this firmware does not change the route automatically.

## Validation

- Native FM adapter: 84 checks, normal and Address/UndefinedBehavior Sanitizer builds; unchanged upstream synthesis fingerprint.
- FM sequencer/MIDI allocation, gates, stealing, six-slot limit and harmonic/rate checks passed with sanitizers.
- Worker fault policy: 102 checks passed with sanitizers.
- Filter-model DSP tests, UNIT-D DSP tests and production project-loader migration/UI checks passed.
- New migration checks cover defaults, all 1–10 saved voice counts in active/all slots, and rejection of corrupt count 11.
- Performance screenshot inspected in host simulator; zero counters there are intentional, not hardware measurements.
- JieLi PI32v2 build passed RAM-resident code, inline 64-bit multiply/load, soft-double and register-access checks.
- Package CRCs, loader, decrypted application bytes and padding verified. No physical FM-1 boot, listening or CPU-load test performed here.

## Build and memory

Package 610108 bytes; app 308312 bytes. Normal RAM 96256 / 98304 bytes (2048 free). Pool 269684 / 344064 bytes (74380 free). Compared with v17.99.8: 16 more bytes free in normal RAM and 1248 more in the pool.

SHA256: `5d0bd0ff74de41c0e5e91bf4201d8f19f5c1080a81289ef9f759528ff19b84e8`

Uses the same DejaVu Sans Condensed font substitutions as the restored baseline. Compiler and verified AC79 SDK resources are external dependencies and are not included in the source archive. See BUILDING-v17.99.9.md.
