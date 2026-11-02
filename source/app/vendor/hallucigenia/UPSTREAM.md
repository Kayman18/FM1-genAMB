# Hallucigenia / Darwinism upstream and FM-1 adaptation

Author: Eater of Sheep, Extratone / XTRTN.
Source: https://github.com/EaterOfSheep/Extratone/blob/main/src/Darwinism.cpp
Pinned source Git blob: 8585735e666ec89b216bc4197ac5bd8d8f0c7335.
Original module listing: https://library.vcvrack.com/Extratone/Darwinism
The listing calls the module Hallucigenia; the original model/file retain Darwinism.
License: GPL-3.0-or-later, as declared by the upstream module listing.
Original Darwinism.cpp and GPL text are included here unchanged.

The freestanding C port is app/dsp/hallucigenia_core.h. The original
randomiseStep, all-16-step mutation loop and advance-before-output clock logic
are adapted from this source, rather than merely reimplementing its concept.
Runtime pitch values use floats as upstream. Tests compile the actual upstream
Module class with a Rack API shim and compare it with the production core.

## Deliberate hardware adaptations

- Two independent instances, Rings and FM, run on CPU0 at clock events.
- System clock and all 13 rate choices replace the Rack clock input.
- End-of-pattern triggers mutation automatically. Upstream expects a separate
  mutation trigger; this is an explicit FM-1 connection, not upstream behavior.
- The original S&H-on-note-trigger order captures pitch before mutation. Sound
  events use the existing FM-1 gate/strike scheduling instead of Rack CV outputs.
- Existing synth humanization (up to 2599 samples of onset delay), Burst and
  voice/gate controls are retained after the sequencer emits a note.
- Notes are quantized to the user's scale or current chord, with octave controls
  applied afterwards. Upstream outputs continuous voltages.
- Existing 16 pattern slots and project format 22 are retained. New randomizes
  a slot; selecting a pattern loads it. Mutations write back to the active slot.
  Project save persists all slots. This replaces separate polyphonic CV save/load
  triggers with the established FM-1 controls.
- Saved note values retain the existing 7-bit pitch + 1-bit gate encoding to
  preserve RC1 projects. Runtime floats are quantized on storage; reloading a
  pattern can therefore slightly change newly generated fractional pitches.
- Hardware restart resets to step zero, and the next clock advances to step 1
  (the second visible step), matching upstream. Old Evolve played step 0 first.
- Rack graphics, expander transport, analog gate/step/end-of-pattern outputs,
  external sample/hold input and dedicated Clear/Reset controls are not exposed
  as new FM-1 UI controls. The port is of the sequencing core, not the full Rack UI.
- Deterministic firmware PRNG supplies uniform floats instead of Rack's PRNG.
  Random sequences are not claimed bit-identical to an arbitrary Rack session.

No changes to Original, Proteus or UNIT-D sequencing algorithms are intended.
