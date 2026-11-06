# genAMB DUSK Monumatic Reverb TEST2

Corrected base: **genAMB DUSK TEST4**, including its DUSK 240x240 UI, generative
pad roles/movement, independent stereo pad filters, Pad1/Pad2/Rings/FM mixer
order, active SEQ/SEL banks and project-format 23 migration/save behavior.
The previous Monumatic TEST1 used an older RC2-WET2-WEB1 base and missed these
changes. User reports that TEST1's replacement reverb sounds good. This build
keeps the exact same reverb core, adapter, control mapping and wet/dry law.

Only reverb integration/UI/metadata and test build identity change relative to
DUSK TEST4. GP5 is replaced, not stacked. The existing mono layer-send bus feeds
Monumatic's stereo output at 44.1 kHz. Delay storage is 90,916 bytes, exactly the
hardware-tested Monumatic TEST22 geometry. No rate reduction or shortened lines.

## FX controls

| Knob | Control |
| --- | --- |
| K1 | Decay, feedback 0.70–0.97 |
| K2 | Tone, damping 100 Hz–13.8 kHz; 85 = 10 kHz |
| K3 | Motion, delay modulation 0–100%; 100 matches Monumatic TEST22 |
| K4 | Dry/Wet; 100 is wet-only |

FX alternates Reverb / Sends. Sends retains TEST4's existing layer sends:
K1 Pad1 / K2 Rings / K3 Pad2 / K4 FM. Mixer retains its different TEST4 order:
K1 Pad1 / K2 Pad2 / K3 Rings / K4 FM.
Start at Decay75 / Tone85 / Motion100 / Mix50, then try 100 decay/mix.
The existing WET2 concave crossfade remains, with the GP5-specific 1.45 return
boost removed (same as accepted-sounding TEST1). Input DC blocker 20 Hz retained.
Panic clears the reverb incrementally; remaining upstream releases can excite
it again. Transport stop allows existing tails.

TEST4 project/snapshot IDs, format 23 and project object size remain unchanged.
Retired GP5 model/damping/rate slots retain legacy ranges, remain ignored by
this reverb and are hidden in the editor. Existing numeric decay/tone/motion
settings are retained. TEST4's format 22->23 pad-filter migration still passes.
The updated editor preserves all 167 IDs, including TEST4's pad-filter keys.

## Build and validation

Build: `sh build-test.sh`; host: `sh build-test.sh --host`.
Set JIELI_TOOLCHAIN to the external jieli-linux-toolchains-20260730.1 directory.
TEST4's bundled DUSK fonts and minimal SDK resources are retained exactly.
Release 17.99.31, display DUSK MONU2, identity FM-1_7179931.
App 304,088 bytes. Normal RAM 96,980/98,304; pool 294,248/344,064;
49,816 pool bytes free, including space for the 8,192-byte platform guard.

Passed target code/memory checks and package CRC/decrypted-payload validation;
ASan/UBSan reverb adapter 4,580,351 frames; full-mix maximum wet/decay render;
reverb/sends navigation and save/reboot restoration; zero-input panic;
DUSK controls, stereo pad filters, mixer placement, format 22 migration and23
save roundtrip; pad roles/movement, sequencer controls, 167-parameter metadata.
Native 240x240 Reverb/Mixer/Pad1 Filter screens were visually checked.
Fresh source rebuild matches firmware bytes (proof in validation folder).
See validation logs and preservation.json for exact retained source checks.

Actual CPU/dropouts/faults and listening still need testing on the FM-1 with
TEST4's pad filters in the mix. The user accepted TEST1's sound; that test used
older synthesis/UI and does not establish this build's worst-case timing.
