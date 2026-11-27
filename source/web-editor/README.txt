genAMB Nature TEST14 (17.99.56 or 17.99.57) - USB-C Web MIDI Editor

1. Connect the FM-1 to the computer over USB-C.
2. Windows: double-click start-editor.bat
   Linux/macOS: run ./start-editor.sh
3. Open the local page in Chrome or Edge (Web MIDI + SysEx required).
4. Click CONNECT USB-C and approve MIDI/SysEx access.

The editor supports all 201 genAMB engine parameters, the 8 pieces, custom
12-note scale pool, lights, transport/freeze, and the non-knob pattern actions.
The browser polls the FM-1 so panel and web edits stay synchronized.

PRESET SNAPSHOTS
- SAVE .GENAMB reads the complete live control state from the FM-1 and saves a
  portable .genamb preset file (JSON internally).
- LOAD .GENAMB restores all 201 parameters plus scale pool, piece identity,
  lights, Play/Rest and Freeze state.
- Loading a file changes the live state only. Press SAVE TO FM-1 if you want
  that state written to the FM-1 project flash.
- The snapshot intentionally does not overwrite the other seven saved piece
  slots or hidden generated note-pattern history; it is a control/settings
  preset, not a full flash backup.

No Internet connection is needed. The web page is completely local.

NATURE
The Nature group exposes Master, Water, Rain, Animals and Bird Density.
On the device, open GLO > Mixer > Nature; SELECT controls density.
Older presets default the five new Nature settings to 0/70/60/55/35.
Use this editor with Nature TEST14 (17.99.56 or 17.99.57).

VA SYNTH
Select Slow Canopy, one of 20 plucks, or one of 20 basses. Factory recall updates the editable oscillator, filter and both envelopes. Layer level, polyphony, sequencer, reverb send and matrix assignments are independent.
M7, M8, M9, M12, M13 and M14 can modulate cutoff, resonance, filter envelope amount, keyboard tracking, filter attack/decay/sustain/release, pulse-width modulation depth and VA volume.

LFO SPEED
M1–M9 retain their slow speed range and add 12 seconds down to 250 ms. M10 and M11 use whole seconds, with 0 for Off. VA Volume modulation can open a zero base level.
