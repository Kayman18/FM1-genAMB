# genAMB 1.0 RC2-WET2 WEB1 — USB-C editor + .genamb snapshots

WEB1 adds a local Web MIDI/SysEx editor to the WET2 firmware.

## Exposed controls

- all 167 `P_*` engine parameters, using the exact firmware parameter IDs
- all 8 pieces
- custom 12-note scale pool
- lights mode
- Play / Rest and Freeze
- Next Chord, Regrow, Reset Piece, Panic and project Save
- New Rings Hallucigenia, New FM Hallucigenia
- New Rings Proteus, New FM Proteus
- CPU meter reset

The browser polls the device so panel changes are reflected in the editor.
State transfer uses short 32-parameter SysEx chunks.

## Preset snapshots

`SAVE .GENAMB` reads a complete live state and downloads a JSON-based `.genamb`
file. `LOAD .GENAMB` restores all 167 parameters plus custom pool, piece
identity, lights, Play/Rest and Freeze. Use `SAVE TO FM-1` after loading if the
snapshot should persist in project flash.

Snapshot restore uses literal parameter blocks rather than one-at-a-time knob
semantics, so changing a model does not overwrite other values while a snapshot
is being restored.

## Browser requirements

Run the included local launcher and use Chrome, Edge or Chromium with Web MIDI
SysEx support. No Internet connection, installer or administrator rights are
needed.
