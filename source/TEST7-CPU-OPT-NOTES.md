# TEST7+ — CPU headroom for the dropout, both editions

Starting point: the **Nature TEST7** kit as shipped, both editions. Both
pristine editions were rebuilt here first and reproduce the shipped packages
**byte for byte** (`62ddf702…` for 256, `31e7c7d8…` for 512), so every number
below is a like-for-like comparison from the same toolchain.

The goal was the dropout, so the brief was: nothing audible may change, the
worst callback matters more than the average, and a layer the player has turned
off may stop rendering.

Eleven files changed, +511 / −61 lines, two of them new tests.

## What it costs now

Host instruction counts for the audio path (x86, callgrind, startup and four
warm-up blocks excluded). `peak` is everything at 100 including Nature;
`silent` is everything at zero; the rest are the oracle's own scenes.

| configuration | 256-frame callbacks | |
|---|---|---|
| **peak** — every layer and effect at 100 | 34,174,090 → **30,758,540** | **−10.0%** |
| **all** — the oracle's full scene | 31,400,312 → **28,171,593** | **−10.3%** |
| wet-max | 31,193,021 → 27,964,106 | −10.4% |
| dry — layers up, every effect at zero | 22,133,839 → 17,207,253 | −22.3% |
| pad2-only (bells + Rings) | 27,468,095 → 23,641,839 | −13.9% |
| fm-only | 28,978,584 → 17,709,850 | −38.9% |
| pad-only | 29,613,651 → 16,821,326 | −43.2% |
| rings-only | 27,428,453 → 15,070,405 | −45.1% |
| nature-only | 21,024,271 → 8,905,391 | −57.6% |
| silent — every layer at zero | 18,250,535 → 6,318,823 | −65.4% |

The two halves of that table are the two different things this does, and which
one matters depends on your piece.

The **−10%** is the floor: it is there whatever you have switched on, and it is
what helps a piece that really does use everything.

The rest is the much bigger lever. TEST7 as shipped spent **18.25 M of the
31.4 M** — 58% — on work that cannot be heard: every layer at zero still
rendered in full. A piece using two or three layers was paying for all of them.
That is now 6.3 M. If the piece that dropped out leaves anything down — no FM,
Rings off, one pad, no filter mix — the saving is in the lower half of the
table, not the top.

**Try the 256-frame edition again first.** The 512 edition traded key and MIDI
latency for a longer deadline; if the headroom above covers the spike, you may
not need that trade. Both editions gain the same amount (the 512 numbers are
within 0.3% of the 256 ones, slightly better on the sparser scenes, because the
block-rate work amortizes over a longer callback).

## Bit-exactness, and the oracle that had to be rewritten

Every change is output-exact. All nine of the oracle's scenes produce **bit-
identical PCM** to the shipped TEST7.

Proving that needed a new test. `genamb_golden_hash` runs all nine scenes in one
process and carries one running hash through them, and `en_init()` does not
reset the pad voices' oscillator phases — so a phase left behind by one scene is
an input to the next. A silent layer whose phase now stops advancing therefore
moves the cumulative fingerprint even though every scene's own audio is
identical, and in that one number a real arithmetic change is indistinguishable
from it. Chasing that down took a while: the first differing scene was `dry`,
where the pad is at 90 and none of the new fast paths ever fire.

`tests/monumatic/run_scene_gates.sh` (new) runs **each scene in its own
process** against its own reference fingerprint. It is strictly the stronger
check, it passes on the shipped tree and on this one, and it is what the claim
above rests on. `run_cpu_gates.sh` keeps its cumulative check with both
fingerprints updated and a comment explaining what it can and cannot see.

## What changed

### 1. The reverb tank — 26% of the peak

`reverb.c` and `monumatic_reverb_port.c` are byte-identical to the RC, so the
work done there came straight across. The per-sample entry did four pointer
guards, a cutoff comparison, and a separate strided pass over all eight delay
lines to re-sum their `filtered` values, before touching the tank. The guards
and the cutoff test are block-rate facts; the sum is exactly the value the
previous sample already computed, line by line, in the same order.
`mm_reverb_begin()` now does the block-rate work once and seeds that running
sum, `mm_reverb_sample()` carries it in a register, and the port peels the arena
clear out of the hot loop and holds the DC blocker and wet ramp in locals.
Measured on the pi32v2 for the RC: **2,257 → 2,083 instructions per sample.**

`-DGENAMB_REVERB_PAIRS=1` enables an unrolled line-pair variant. It is off,
because on the real ISA it buys 0.4% for 1,232 bytes and twice the hot-loop
footprint — the host said 1.5%, and the host was wrong.

### 2. The classic filter bank stops running when it is dry — 19% of the floor

`classic_filter_stereo_sample` ran four control smoothers and a blend ramp per
sample per layer, loading from the struct and storing back; and when the bank is
fully dry it still ran the filter, with `ff_mix()` then handing back the dry
input because `mix = wet * blend` is zero. A dry bank is the normal state of both
Pad layers unless a filter mix has been dialled in, and it was costing a fifth
of the engine.

`classic_filter_stereo_block()` takes the whole block. If the bank is dry for
certain across it — `wet` and `target_wet` both exactly zero, the blend ramp
parked, no topology change pending — it does nothing at all, because `mix` is
zero for every sample of such a block. Otherwise the five smoothed scalars live
in a local the compiler keeps in registers, with every test and operation in its
original order. The one behavioural difference is that a dormant bank's
coefficients stop tracking; on the first block that is not dry they are snapped
to their targets, which is where tracking nothing for a few hundred samples had
already put them.

### 3. Pad 1 gets the fast path Pad 2 already had — the biggest peak win

TEST7's own notes say "resting Pad 2 voices keep their oscillator phase while
skipping inaudible work". Pad 2 has that; **Pad 1 was left out.** Same condition,
copied verbatim: the layer's output gain exactly zero, or a voice whose target is
zero and whose envelope has fallen below −360 dB.

This is the single largest change in the top half of the table — `all` went from
−4.1% to −10.3% on its own — because pad voices are resting much of the time
even when the layer is loud, so it is a peak win and not only a mute win.

The phases and the breath envelope still advance exactly, so the meters stay
right and a voice is where it would have been; only the warm low-pass state and
the sub's smoother hold still. `pad_coefs()` keeps running either way, and that
matters more than it looks: `pad_breathe()` draws from the engine's shared
xorshift, so skipping it would have moved **every other layer's notes** — you
would have heard a different piece, not a quieter one. Every skip in here is
placed to leave that stream alone.

### 4. Rings stops rendering at level zero — 17% of the peak when it is down

`rings_port_render_add` ran `g_part.Process` — four voices of modal filters —
and then scaled the result by `g_level` on the way to the buses. At level zero,
and while muted where the mix is skipped outright, every sample it produced was
multiplied by zero. Now it is not computed. The pending strum is still consumed,
so strums cannot pile up and fire together when the level returns, and Rings has
its own RNG so no other layer moves. Note that Rings' level is the **BELLS**
control, so a piece with Bells down was paying for Rings in full.

### 5. FM stops when it is silent *and* idle

The same idea for FM needed a second condition, and the repository's own tests
are what caught it. Gating only on silence skipped `make_block()` — which is
also where a timed note's gate counts down and where a finished voice is
retired. Three of `run_fm_port_tests.sh`'s gate-timing cases failed, and rightly:
notes would have hung for as long as the layer stayed down. With the liveness
test added there is no gate to count and no voice to retire, and all 89 pass.

It fires less often than the others, because the FM sequencer keeps voices live
even at level zero — it is the piece that never uses FM that gets the 11% back.

### 6. Smaller, bit-exact

- **Resonator lane accumulators.** `AddFixedModes` read and wrote `odd[j]` and
  `even[j]` once per lane pair; they are now two registers across the pairs, in
  the same order. The rest of that function is already the right shape — the
  mode-outer blocking with a static-position fast path is a better answer to it
  than the one I tried and rejected on the RC, and `ComputeFilters` is already
  memoized on the same five keys.
- **`sine_wrapped()`.** For a phase an oscillator has already wrapped into
  [0, 1), `ph - (float)(int)ph` is two conversions and a subtract for nothing.
  The range test is the contract's guard, not an approximation: anything outside
  falls through to the full reduction, so the table index cannot leave its
  bounds.
- **Nature's gain ramps.** `nature_smooth()` is a no-op for a gain whose ramp has
  finished and only control rate can start one, so four loads and four branches
  per sample became one test per block.

## Measured and rejected

- **Hoisting Pad 1's per-sample derived terms** (`sawmix * 0.92f`, the output
  trim, the sub's half step) and fusing its two bus read-modify-writes. These
  won slightly on the RC; here they **cost 0.15%**, because this version of
  `render_pad` has more live state and the extra values spill. Not kept.
- The RC's own rejected list still applies: mode-outer blocking of a
  position-interpolating resonator, hoisting per-voice coefficient loads, and
  duplicating loops to lift a per-sample test — all measured slower, on the host
  and, for the last two, on the pi32v2 disassembly as well.

## Still on the table

- **The reverb tank**, 26% of the peak and arithmetically minimal: the band-pass
  is five multiplies and seven adds with no bit-exact reduction available, and
  the cubic interpolation per line is about twenty operations that all feed the
  output. Running it at half rate would change the sound.
- **FM at level zero with the sequencer running.** The synthesis is unavoidable
  today because the envelopes and gates advance inside `fm6_note_compute`.
  Splitting that into advance-and-render would recover roughly 11% whenever FM is
  down, and it is the largest remaining authorised saving — but it means
  reaching into the vendor core, which carries the AC79 64-bit-multiply erratum
  the build audits for.
- **Nature's per-sample branches.** `master` and the three layer gains are
  block-constant whenever no ramp is travelling; hoisting them needs the loop
  duplicated, and duplicating loops has lost every time it has been measured
  here.
- **Spikes as such.** Everything above lowers the work; nothing spreads it. The
  remaining block-rate lumps are `make_block()`, an animal call starting (a
  flash read and an ADPCM seek), and a Rings note forcing `ComputeFilters` to
  recompute. On 512-frame callbacks those already amortize over twice the
  deadline, which is what the 512 edition was for.

## The device builds

Both editions, the kit's own `20260730.1` toolchain, `sh build.sh` and
`sh build.sh --buffer-512`. All five target audits pass on both, including the
AC79 erratum check, and both pass `package.py` in full — CRCs, loader, decrypted
app, padding, the embedded bank and the three external recordings.

| | 256 edition | | 512 edition | |
|---|---|---|---|---|
| | TEST7 | this tree | TEST7 | this tree |
| app image | 532,380 B | 534,236 B | 532,412 B | 534,268 B |
| | | +1,856 B (+0.35%) | | +1,856 B (+0.35%) |
| RAM `.data`+`.bss` | 50,012 / 98,304 | 50,028 | 60,252 / 98,304 | 60,268 |
| pool | 283,500 / 344,064 | 283,500 | 283,500 / 344,064 | 283,500 |
| package | 741,286 B | 741,286 B | 741,286 B | 741,286 B |

RAM moves by 16 bytes — one `dormant` flag per filter layer and padding. The
pool does not move. Package sizes are unchanged.

```
genAMB-Nature-TEST7opt-256-17.99.40.fwsc  SHA256 5a9792dde9b9b97cce3c4e7e12f3d1fe050999010a82b451a20c861df9f417b9
genAMB-Nature-TEST7opt-512-17.99.41.fwsc  SHA256 92075d89386d06309bea2da2f869e75844a3836a29c4614b3b1fecd3ff192b65
```

Display names and identities are unchanged (`N7-256`/`FM-1_7179940`,
`N7-512`/`FM-1_7179941`), so they install over TEST7 with the usual updater and
keep the same project format and wave bank.

## Verification

Run on this tree and on the pristine one, and compared.

- `tests/monumatic/run_scene_gates.sh` (new) — **all nine scenes bit-identical**
  to the shipped TEST7, each in its own process. This is the bit-exactness gate.
- `tests/monumatic/run_cpu_gates.sh` — both fingerprints pass (updated, with the
  reason recorded in the script); the reverb sleep/wake gate finds finite audio
  across every DRY/WET change.
- `tests/monumatic/run_adapter_test.sh` (ASan + UBSan) — `PASS frames=4580351
  peak=0.0331464 energy=1.33899`, the same figures the RC tree gives.
- `tests/run_fm_port_tests.sh` — **89 passed, 0 failed.** These are the tests
  that rejected the first version of the FM gate; they are the reason it has a
  liveness condition.
- `sh build.sh` and `sh build.sh --buffer-512`, then
  `python3 source/tests/nature/package.py 17.99.40` / `17.99.41` — all audits and
  all package checks pass.
- `tests/run_tests.sh` fails identically on both trees: `tests/scenarios/*.amb`
  is not in the kit. Not a regression.
- The pristine tree was also run under ASan and UBSan at `-O1` and is clean, and
  its fingerprint does not move between `-O1` and `-O2` — so none of this leans
  on a particular optimisation level.

## Not verified

**Nothing has been run on an FM-1.** No timing, no Audio Avg or Audio Peak, no
Dropouts or Faults count, no listening. Host wall-clock timing was tried and
discarded as too noisy to mean anything (a 4.4 ms outlier from the scheduler on a
callback whose deterministic instruction count had gone *down*); the figures here
are instruction counts, and the one pi32v2 figure is a static count off the
disassembly, not a clock.

The test is the Performance page, as the kit's README describes it: same piece,
same mixer, same Rings model and core mode, **GLO** until Performance, **K2** to
reset, play at least as long as TEST6 took to drop out, then read **Audio Avg**,
**Audio Peak**, **Dropouts**, **Faults**, and **PK C0 / PK C1 / PK WAIT**.

That last trio is the thing I could not determine from here and that decides
where any further work should go. CPU0 blocks on CPU1, so the callback is
whichever side is longer. Pads, Nature, the filter bank and the reverb are CPU0;
Rings and FM are CPU1. The reverb and the pads are the two biggest items and
both sit on CPU0, so if **PK WAIT** is large then CPU1 is your critical path and
most of the −10% is landing on the side that was already waiting — in which case
the Rings and FM savings above are the ones that matter, and the next thing worth
doing is the FM advance-and-render split rather than anything on CPU0.
