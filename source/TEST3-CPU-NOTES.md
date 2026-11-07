# TEST3 — CPU headroom

Starting point: the DUSK Monumatic Reverb TEST2 tree in this repository. The
goal was headroom without changing how the instrument sounds.

Everything below is **bit-exact** — identical output sample for sample —
except one item that is marked, measured and switchable.

**This is the corrected build.** The first TEST3 attempt also moved the audio
path into executable SRAM; that faulted at boot on the real unit and is now off
(section 2 below has the fault details and the switch). Nothing else changed:
the 18.2% reverb saving and the dead-send removal are arithmetic and placement
of data, not of code, and both are intact. The golden hash still matches TEST2.

## Where the time goes

Measured as host instruction counts (x86, callgrind), startup excluded, all
layers and effects up, rendering 40 blocks of 256 frames:

| | share of the audio path |
|---|---|
| Rings resonator (`Resonator::Process`, its SVF and cosine-oscillator loops) | 35% |
| Monumatic reverb (`mm_reverb_process` + `reverb_math.h`) | 17.6% |
| Pad 1 (`render_pad`) | 11.8% |
| Pad filters (`classic_filter_sample` + `ff_svf`) | 11.3% |
| Pad 2 (`render_pad2`) | 9.25% |
| Rings `Part` and the port | ~4% |
| `en_render` / mixing | ~1.6% |

That is about 5,000 instructions per output sample with everything running.

## The thing worth knowing first

**Every layer and the reverb ran at full cost regardless of its level.** Four
configurations measured within 7% of each other:

| configuration | instructions / 40 blocks |
|---|---|
| everything on | 52,427,331 |
| pads only (Rings and FM levels at 0) | 48,706,201 |
| Rings only (both pad levels at 0) | 50,298,483 |
| fully dry (every reverb send and DRY/WET at 0) | 50,933,090 |

`render_chunk` calls `render_pad`, `render_pad2`, `dc_render_rings` and
`monumatic_reverb_process` unconditionally; a level of zero only multiplies the
result by zero afterwards. One of those four is addressed here (the reverb); the
others are described under **Still on the table**.

## What changed

### 1. Dead reverb sends removed (bit-exact)

Four per-sample sites were accumulating into `esend[]` — `pad_bus`, `pad2_bus`,
the bells and the hats. But `monumatic_reverb_process(rsend, esend, n)` takes
the mono send in `rsend` and *writes* wet left into `rsend` and wet right into
`esend`, assigning every element before it is read. Those sends were left over
from an earlier stereo-send design and could never reach the output. Removed,
along with the per-block clear of a buffer that is now write-only.

### 2. The SRAM placement — built, then reverted after a hardware fault

**This is off in this build.** It is described here because it is the one piece
of TEST3 that did not survive the real part, and because the switch to turn it
back on is still in the tree.

The idea: the RAMTEXT window (`0x01C00000`, 24,576 bytes) holds only
`.ram_text` — 2,968 bytes of flash driver — while the entire 44.1 kHz path
executes in place from the flash over XIP, competing for the flash interface
with the wavetable, mode and resonator-coefficient reads it makes on every
sample. Moving the per-sample code into a `.dsp_hot` section copied to RAM at
boot should remove those fetch stalls. It linked cleanly (`.ram_text` 2,968 +
`.dsp_hot` 16,380 = 19,348 of 24,576) and every host check passed.

On hardware it faulted at boot: `VEC 00000001`, `PC 01C04BAA`, `DBG 00001000`
— a PC-limit violation, with the program counter 22 bytes past `_dsp_hot_end`
(`0x01C04B94`), returning from the indirect call in `en_render` that loads
`render_pad_ptr`. Execution ran off the end of the copied section.

I verified, in the linked image, that the crt0 copy loops for both `.ram_text`
and `.dsp_hot` were correct, that the three stored entry pointers held the right
addresses (`0x01C01AD4`, `0x01C0320C`, `0x01C0478A`), that the second PC-guard
window covered `[_rt_start, _dsp_hot_end - 1]`, and that the image was large
enough to carry the load data. All four were right. **The cause is not
identified**, so the placement is off rather than shipped on a guess.

The one structural difference against the sibling V-Station firmware, whose
equivalent section has been proven on hardware: V-Station's hot section is
pre-existing and executes only up to about `0x01C02358`, roughly 9 KB in,
whereas this one ran out to `0x01C04B94`.

What is still in the tree, unused:

- `.dsp_hot` in `base/device/app.ld`, placed after `.ram_text`, `AT > XIP`
- the boot copy in `base/device/main_fm1.c`
- the extended guard window in `base/bsp/chip/fm1_guard.h`
- `PLAT_HOT` in `base/include/plat.h`, and the `PLAT_HOT` markers on the
  audio-path functions
- the force-inlining of the small shared helpers (`rnd`, `frand`, `mod_value`,
  `breath_advance`, the `fastmath` entries, stmlib's `Svf`, `CosineOscillator`,
  `ParameterInterpolator`, `Interpolate`, `min`/`max`) and the volatile
  function pointers that cross the gap (`render_pad_ptr`, `render_pad2_ptr`,
  `monumatic_reverb_process_ptr`, `en_midi_out_ptr`,
  `rings_resonator_process_ptr`)

`PLAT_HOT` expands to nothing unless `PLAT_HOT_ENABLE` is defined, so with the
placement off all of that is inert: the section links at zero bytes, the copy
moves nothing, and the guard window is back to `.ram_text` alone. Building with
`-DPLAT_HOT_ENABLE` restores the attempt. If you want to try it again, do it
incrementally — one function at a time, or start with just the reverb tank, at
roughly V-Station's proven footprint — and check each step on the unit.

The inlining and the pointer indirection are harmless on their own: inlining a
small helper is a win either way, and each pointer call happens once per block
or per voice. The two edits inside `app/vendor/rings/` are placement and the
trampoline only. Emilie Gillet's arithmetic is untouched.

### 3. The reverb tank sleeps when DRY/WET is at zero

**This is the one item that is not bit-exact, and it is switchable.**

At DRY/WET = 0 the wet return is multiplied by zero, so the tank cannot reach
the output: eight interpolated delay lines per sample are computed and thrown
away. It is now skipped, together with the peak loop and the crossfade — the
meter reduces to `en_reverb_level * 0.92` once `peak` is multiplied by zero, and
the crossfade reduces to the dry mix, so **the output and the meter are
identical** while the reverb is down.

Measured: **50,933,090 → 41,685,954 instructions, −18.2%**, with everything-on
unchanged (52,427,331 → 52,306,484).

What differs: on the TEST2 build the tank kept running while muted, so raising
DRY/WET from zero revealed a tail that was already in flight. Now the tank is
cleared when it wakes and the reverb blooms in instead. Measured over the first
232 ms after the knob comes up, the wet energy is 0.35 against TEST2's 5.81 —
audible as a slower bloom, not as a defect, and the steady state is identical.

To keep the old feel, build with `-DGENAMB_REVERB_SLEEP=0`. The output while
the reverb is down is the same either way; the difference is the 18% and the
bloom.

### 4. A latent register-corruption risk — already clean here

Worth noting because the other two FM-1 firmwares needed it: this build's audit
already reports `no 64-bit multiply inlined beside a load`, so the felucca-bsp
CPU erratum is already handled. No change needed.

## Verification

`tests/monumatic/run_cpu_gates.sh`:

- `genamb_golden_hash.c` — nine configurations (everything on, each layer alone,
  fully dry, two Rings models, fully wet), each seeded, played, struck with
  Rings and FM notes, advanced to the next chord, frozen, stopped and panicked.
  One hash of every output sample. Expected `GAGOLDEN 963138e825b936e9`, which
  is the TEST2 output; it has matched after every change here, including the
  dry configuration that exercises the sleep path.
- `genamb_reverb_wake.c` — drives DRY/WET 0 → 80 → 0 → 80 and requires finite
  audio on both sides of every transition.

Also run and matching the pristine tree:

- `tests/monumatic/run_adapter_test.sh` — PASS, `frames=4580351 peak=0.0331464
  energy=1.33899`, byte-identical to TEST2's result.
- `sh build-test.sh` — target audit clean on every check.
- `sh tests/run_tests.sh` — `ok host-build`, then the same
  `FAIL scenario-*` as the unmodified tree: `tests/scenarios/*.amb` are not in
  the repository, so that gate cannot run here. It is not a regression.

## Still on the table

Ordered by what they would be worth:

1. **Rings and FM when their levels are 0** — about 1.7 M instructions per 40
   blocks each. `rings_port_set_level`/`fm_port_set_level` scale the output, so
   skipping is output-exact, but the strings and operators would stop advancing:
   notes held while muted would not be there on unmute, and `rings_port` has no
   activity query to make the skip conditional on genuine silence
   (`fm_port_active_voices` exists for FM). That is a behaviour decision, so it
   is not done here.
2. **Pads when their level is 0** — same shape. `render_pad` already skips
   individual voices once their envelope and the gate have decayed, so the
   remaining cost is a playing pad whose fader is down.
3. **The string models in SRAM** — moot while the SRAM placement is off. If it
   is ever made to work, `String::Process` and its two `ProcessInternal`
   instantiations are 6,304 bytes, which did not fit beside the 16,380 the rest
   of the path took; dropping `render_pad2` would have made room.
4. **Per-sample smoothing in `classic_filter_sample`** — four one-pole updates
   per sample per filter instance, before the bypass test. Reordering them is
   not free: the smoothers must keep tracking or the filter clicks when it comes
   back, so this needs care rather than a quick edit.

## Not verified on hardware

The reverb sleep's 18.2% is a host instruction count, not a device reading, and
nothing in this build has been measured on the unit. What *has* been settled on
hardware is that this build boots: the change that did not, the SRAM placement,
is out.

The perf page is the test: with both pads, Rings, FM and the reverb up, compare
the CPU reading and the xrun counter against a TEST2 build, then again with
DRY/WET at zero to see the reverb sleep — that second reading is where the 18%
should show.

## Build

```
sh build-test.sh
```

Needs `JIELI_TOOLCHAIN` pointing at the JieLi Linux toolchain and a Python with
Pillow. Output: `app/build/amb-17.99.31.fwsc`, 610,108 bytes, identity
`FM-1_7179931`. Audit summary from this tree:

```
ok  .ram_text: 929 insns, no calls
ok  no 64-bit multiply inlined beside a load
ok  no soft-double routines (single-precision FPU only)
ok  image 304876 B; RAM .data+.bss 96980 B of 98304; pool 294248 B of 344064
ok  register access: felucca-bsp only
```
