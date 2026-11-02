# Three retained Rings models: native regression

This test compares the actual pending Rings adapter and DSP with an unchanged
**genAMB v17.99.2 FM16** source tree. It builds small native executables only;
it does not build the application or device firmware and never writes to
`app/build`.

## Run

From the pending source root:

```sh
python3 tests/next-build/rings-three-models/run.py /absolute/path/to/genAMB-v17.99.2-FM16
```

To retain test binaries and fresh logs in an isolated directory:

```sh
python3 tests/next-build/rings-three-models/run.py /absolute/path/to/genAMB-v17.99.2-FM16 --out /tmp/rings-three-check
```

The candidate defaults to the repository containing the runner. `--candidate`
can override it. `CC` and `CXX` may specify native compiler executable paths.
The runner needs Python 3, a C99 compiler, and a C++11 compiler. Floating-point
contraction is disabled, matching the native application build.

## Coverage

- The shared helper header compiles as C99 and C++; all seven historical IDs,
  three selectable ordinals, and integer clamp endpoints are checked.
- The actual adapter and `Part.set_model()` resolve legacy IDs 3, 5, and 6 to
  String (2), and ID 4 to Sympathetic (1).
- Model modulation reaches all three retained models, retains hysteresis,
  commits on the next strike, and applies the existing 1,323-sample fade.
- Modal, Sympathetic, and String are compared at polyphony 1, 2, 3, and 4.
  Each case renders 96,000 stereo frames plus its reverb-send stream, with the
  existing Rings delay enabled. All float samples must be finite, the signal
  must be nonzero, and all three streams must match the release byte for byte.
- Each legacy mapping is also compared with the corresponding released
  replacement model at all four polyphony settings.
- The global Clouds reverb is compared independently for 192,000 stereo frames.

The included `results.txt`, `audio-hashes.json`, and `source-provenance.json`
record the validated pending source. Full PCM and compiled binaries are not
included in this test directory. The source hash manifest identifies the
compared baseline and candidate files.

## Memory accounting

Native 64-bit `sizeof` results are measured by the harness:

| Object | Released | Pending | Reduction |
|---|---:|---:|---:|
| Rings `Part` | 110,704 B | 110,048 B | 656 B |
| Removed `StringSynthPart` object | 2,104 B | 0 B | 2,104 B |
| Removed shared effects buffer | 65,536 B | 0 B | 65,536 B |

The native result must not be used as the device total because pointer and
`size_t` widths differ. The released **target ELF** provided the device sizes
`g_part = 110,452 B` and `g_string_synth = 2,052 B`. Target field accounting is:

- Four removed FMVoice objects: 4 × 144 = **576 B**.
- Removed Part reverb: a 40-byte FxEngine (one 4-byte write index, one 4-byte
  buffer address, two 16-byte cosine oscillators) plus seven floats = **68 B**.
- Pending target Part: 110,452 − 576 − 68 = **109,808 B**.
- Total removed pool object storage: 576 + 68 + 2,052 + 65,536 = **68,232 B**.
- Additional ordinary RAM removed: the mutable quantized-chord table
  (4 × 11 × 8 floats = 1,408 B) and the obsolete `g_string_fx` integer (4 B),
  for **1,412 B**.

These are target object-layout savings derived from baseline ELF sizes and
32-bit field layouts. No candidate device image was built for this test; a
future device link will establish final section sizes including alignment and
other pending changes. The global Clouds buffer, Rings delay buffers, and their
processing are retained.

## Change scope

Production changes cover `app/rings_port/rings_models.h`,
`app/rings_port/rings_port.h`, `app/rings_port/rings_port.cc`,
`app/vendor/rings/rings/dsp/part.h`, `app/vendor/rings/rings/dsp/part.cc`,
`app/vendor/rings/rings/dsp/plucker.h`, and `app/fm1.toml`.

`plucker.h` only qualifies three existing stmlib names; the removed FM header
previously made them visible through a namespace directive. Model IDs 0, 1, and
2 are unchanged. Historical IDs remain accepted for project migration.
Model modulation now spans the three retained models, so trajectories that
previously visited removed models intentionally change. Its hysteresis,
next-strike scheduling, and fade are preserved. The obsolete String FX adapter
setter is retained as a no-op for old project parameter call sites.
