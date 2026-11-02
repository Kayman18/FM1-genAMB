# genAMB FM-1 1.0 RC2 — upstream Hallucigenia port

Replaces the independent Hallucigenia/Evolve implementation for BOTH Rings and FM with an adaptation of Eater of Sheep / Extratone's original Hallucigenia sequencing core (`Darwinism.cpp`). Original, Proteus and UNIT-D remain selectable. All RC1 UI and USB branding changes are retained.

## What changed

- Source-derived random-note/density logic and all-16-step probabilistic mutation.
- Clock advances before reading the output, as upstream: after a reset, the first clock plays step 1 (the second visible step). RC1 played step 0 first.
- Mutation affects all 16 steps, including steps outside the current playback length.
- Pitch is captured before mutation on the same clock event, matching upstream S&H-on-trigger behavior.
- Independent runtime floating-point note state for Rings and FM. No allocation in the audio path.

## FM-1 adaptations retained

The upstream sequencer expects separate clock/mutation inputs. The FM-1 system clock drives it, and end-of-pattern is connected to mutation automatically. All 13 Rate choices, scale/chord following, octave controls, humanization, Burst, and existing synth gate/voice controls remain. Humanization can delay audible note onsets by up to 2599 samples; the source comparison tests concern the sequencer core, not a claim of identical audio to VCV Rack.

The existing 16 pattern slots remain. New randomizes a slot; selecting a pattern loads it, mutation writes back to that slot, and project Save persists the banks. Project format remains 22, with existing RC1 banks loadable. Saved pitch values retain the original 7-bit encoding; runtime floats are quantized when stored. Recalled generated patterns can therefore differ slightly at harmonic quantization boundaries. This is a sequencing-core port, not a port of the Rack GUI, CV patching or expander. See app/vendor/hallucigenia/UPSTREAM.md for all adaptations and omitted CV/UI features.

## Validation

- 10000 comparisons against the actual vendored upstream Module class, using a Rack API shim: randomization, density, mutation, random-number consumption, step lengths 2–16, clock order and S&H order matched.
- All 256 legacy packed note/gate byte values round-trip through the new runtime state.
- Production Rings and FM integration tests: first-step/wrap behavior, all-16 mutation at length 2, RC1 bank restoration and 49152 scale/chord mappings passed.
- Sanitized FM sequencer regression suite: all 13 rates with internal/external clocks, stop cancellation, 138240 harmonic mappings, existing Original loops, MIDI gates and 1–6 voice behavior passed.
- UNIT-D regression suite, existing UI/navigation tests and project/filter migration suite passed.
- Hardware cross-build and memory/code checks passed; package CRCs, loader, decrypted application and padding verified.
- No physical FM-1 boot, listening or CPU timing measurement was performed here.

## Suggested hardware check

Load an existing project, select Hallucigenia on Rings and FM, and try Mutation at 0 and 100. Check New, pattern switching, Save/reboot, rate changes and scale/chord follow. At a short length, mutation now also changes hidden steps; extending Length should reveal those changes. Playback starts on the second visible step after reset, matching upstream.

## Identity and resources

Display: 1.0 RC2. USB: genAMB / genAMB FM-1.
Updater identity: FM-1_7179913; internal build release 17.99.13.
Firmware: 610108 bytes; app: 312472 bytes.
Normal RAM: 96400 / 98304 bytes (1904 free), 144 bytes more than RC1.
Pool: 269684 / 344064 bytes (74380 free), unchanged.
SHA256: `a8e8e4e7e896b51a5bd041db71945cad4caeee982b87026e38487c7e37399b9e`

FM maximum remains six voices, default four. The FM filter remains on CPU1. The previously reported high-octave FM loudness issue remains unresolved.

## Source credit

Hallucigenia / Darwinism by Eater of Sheep (Extratone / XTRTN):
https://github.com/EaterOfSheep/Extratone/blob/main/src/Darwinism.cpp
Pinned Git blob: 8585735e666ec89b216bc4197ac5bd8d8f0c7335.
Upstream source, GPL license and adaptation notes are included in the source archive.
