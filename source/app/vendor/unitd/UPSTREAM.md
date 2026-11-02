# UNIT-D source and hardware adaptation

Original software: **UNIT-D / UnitDistanceSeq**, by Animatek, in
[`animatek/UZZ-VCV-RACK`](https://github.com/animatek/UZZ-VCV-RACK/blob/main/src/UnitDistanceSeq.cpp).
The upstream repository is GPL-3.0-licensed; the `app/dsp/unitd.c` adaptation is
SPDX GPL-3.0-or-later. Copyright to the respective contributors remains with them.

Adapted features: deterministic unit-distance node geometry, Radius/Tolerance/
Density connection topology, Seed, 8–64 Nodes, three Walk algorithms, degree-
weighted gates, 16/32-step bipolar partial/full Lock, four-octave range, Gate
Length and Gate Density, per-voice graph walk and accent-derived velocities.

The FM-1 implementation replaces the VCV virtual CV pitch output with the
current genAMB Original-progression chord quantizer. The optional VCV patch
inputs and standalone X/Y/ACC jacks are not exported on the FM-1; the node X
position supplies pitch and node degree supplies velocity. Graph and capture
buffers are fixed-size and placed in audio-pool RAM. On-device sequencer
polyphony is limited to 1–4 Rings notes or 1–8 FM notes (the FM synth supports
up to 10 total voices).

The saved project retains UNIT-D controls, deterministic Seed and compact
walker state. The full live 16/32-step captured lock-loop history is not saved;
if a project boots with LOCK engaged, an equivalent new deterministic capture
is seeded from the saved state to avoid silence. The restarted phrase might
not be identical to the live recorded capture when it was saved.

Firmware is a GPL-3.0-combined distribution with MPL-2.0 and permissive
components; see the root LICENSING.md and the accompanying GPL copy.
