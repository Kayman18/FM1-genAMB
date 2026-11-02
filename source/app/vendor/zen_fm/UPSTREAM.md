# ZEN-1 FM core used by genAMB v17.99

Source repository: https://github.com/aceyprime/zen-1

Pinned commit: `ec9295233ea64ddac224b58b4d07216706c1b8ee`.

The complete imported source and the relevant generators are supplied here.
The firmware builds the adapter in `app/fm_port/fm_port.c`, which includes the
core directly. The original platform, scene UI, twelve-voice instrument wrapper,
nature recordings and effects are not imported.

## Licenses and attribution

`fm6_core.c` is Felucca's integer C port of msfa, licensed Apache-2.0:
Copyright 2012 Google Inc.; Copyright 2016–2025 Pascal Gauthier;
C port 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments.
The full text is in `LICENSE-Apache-2.0-msfa.txt`.

`fm6_patch.c`, `fm6_tables.h`, their generator, patch-generation helpers and
ZEN-1's own nine ambient patches are GPL-3.0-only. The original declarations
remain in the files, `LICENSE-GPL-3.0.txt` supplies the full text, and
`UPSTREAM-LICENSING.md` preserves ZEN-1's detailed attribution.
The new adapter, factory-table generator and associated tests are GPL-3.0-only.

These GPL components are supplied with the complete corresponding source for
this combined firmware. The other components retain their own MPL-2.0, MIT,
Apache-2.0 and font-license notices; this import does not relicense their authors'
files or erase their notices. See the top-level `LICENSING.md` for the combined
firmware's distribution terms.

## Imported files

- `fm6_core.c`, `fm6_tables.h`, `fm6_patch.c` and `zen_patches.h` originated in
  `firmware/src/dsp/`.
- `gen_fm6_tables.py`, `zen_patches.py` and `fumi_patches.py` originated in `tools/`.
- `fumi_patches.py` is reduced to the three helpers needed by `zen_patches.py`.
  The separate FuMi/community patch bank is omitted.
- `gen_factory.py` is new. It emits the same nine voices in expanded form as
  const flash data, allowing sounding voices to retain their selected patch
  without keeping a RAM patch copy per voice. Tests compare all 156 expanded
  bytes and all 128 packed bytes against the original `zen_patches.h` records.

Regenerate from the project root:

```
python3 app/vendor/zen_fm/gen_fm6_tables.py app/vendor/zen_fm/fm6_tables.h
python3 app/vendor/zen_fm/zen_patches.py app/vendor/zen_fm/zen_patches.h
python3 app/vendor/zen_fm/gen_factory.py app/fm_port/fm_factory.h
```

## Changes to the core

The algorithm routing, audio tables, envelope formulas and original patches are
retained. These portability fixes are marked in `fm6_core.c`:

- Phase modulation uses explicit unsigned 32-bit wraparound.
- Multiplication replaces left shifts of possibly negative values in velocity,
  key scaling and the sine LFO.
- All 32-by-32-to-64-bit products are isolated in small `noinline` helpers.
  On this FM-1 CPU, interleaving those products with memory loads can corrupt a
  register. See `base/bsp/docs/CPU.md`; the device disassembly check must remain
  enabled. This is a hardware stability measure, not a checker exclusion.
- The amplitude-modulation multiply uses the algebraically identical
  `(level * pt) >> 24` form instead of `(level * (pt << 4)) >> 28`.
- The outdated 244-byte note-size comment is corrected to the measured
  296 bytes. The structure itself is unchanged.

## Adapter behavior and limits

- Four independent voices, with released-voice-first, then oldest-voice stealing.
- A preset change preserves sounding notes on their original preset; each of
  the nine presets has a shared LFO, matching ZEN-1's per-part arrangement.
- Manual notes use a zero sample-count gate and release on explicit note-off.
  Sequenced notes release after the supplied gate and then follow the original
  patch's envelope. A gentle stop releases all notes; panic clears pending sound.
- The core runs at 44.1 kHz in 32-sample blocks. Cached output is consumed once,
  including when the caller splits at irregular event boundaries. Event latency
  is at most 31 samples (0.703 ms); nonzero gates round up to a block boundary.
- The voice and LFO pool state occupies 1,464 bytes. Shared DSP/output buffers
  occupy 512 bytes of normal RAM, plus about 40 bytes of controls and meters.
- Factory voices use DX7-format data, and the format codec and generators are
  included for a future importer. This build has no runtime DX7 SysEx receiver,
  user-patch storage, or bank import UI.
- The 32 algorithms are the upstream msfa/Felucca algorithms, including their
  existing approximation of feedback routing for algorithms 4 and 6; no claim
  of bit-exact Yamaha hardware emulation is made.

Run the focused adapter and sanitizer checks with:

```
sh tests/run_fm_port_tests.sh
```

Host results verify DSP correctness and state management. They do not establish
the hardware's worst-case two-core timing margin.
