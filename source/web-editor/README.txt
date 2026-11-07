genAMB RC - USB-C Web MIDI Editor

1. Connect the FM-1 to the computer over USB-C.
2. Windows: double-click start-editor.bat
   Linux/macOS: run ./start-editor.sh
3. Open the local page in Chrome or Edge (Web MIDI + SysEx required).
4. Click CONNECT USB-C and approve MIDI/SysEx access.

The editor exposes all 167 genAMB engine parameters, the 8 pieces, custom
12-note scale pool, lights, transport/freeze, and the non-knob pattern actions.
The browser polls the FM-1 so panel and web edits stay synchronized.

PRESET SNAPSHOTS
- SAVE .GENAMB reads the complete live control state from the FM-1 and saves a
  portable .genamb preset file (JSON internally).
- LOAD .GENAMB restores all 167 parameters plus scale pool, piece identity,
  lights, Play/Rest and Freeze state.
- Loading a file changes the live state only. Press SAVE TO FM-1 if you want
  that state written to the FM-1 project flash.
- The snapshot intentionally does not overwrite the other seven saved piece
  slots or hidden generated note-pattern history; it is a control/settings
  preset, not a full flash backup.

No Internet connection is needed. The web page is completely local.
