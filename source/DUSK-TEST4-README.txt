genAMB DUSK TEST4 - stereo pad filters and mixer reorder

One shared stereo filter per pad, after each pad voice mix and before its
main output and reverb sends. Both pads retain independent L/R histories.
The filter is removed from the FM audio path and SEL page cycle.

Pad 1: press ENV through Pad 1 -> Body -> Pad 1 Filter -> Pad 1 Seq.
Pad 2: press ARP through Pad 2 -> Body -> Pad 2 Filter -> Pad 2 Seq.
Filter pages open with four active controls:
  K1 Type, K2 Cutoff, K3 Resonance, K4 Mix.
  SELECT switches to the model bank; K1 chooses the model.
  Models: Bypass, Clean SVF, SEM, Juno, Polysix, CS-80.
  OCT-/OCT+ on a pad filter page controls that pad's octave.
Both filters start as Clean low-pass. Motion applies independent cutoff
movement using each pad's existing slow cycles. M10/M11 now modulate the two
pad filters' cutoff/resonance instead of the removed FM filter.

Mixer: K1 Pad 1, K2 Pad 2, K3 Rings, K4 FM.
SEQ and SEL sequencer mode/bank controls from TEST2 remain.
Pad roles/movement and all ten GP5 reverbs remain.

CPU comparison (same development host, low-pass mode, control updates every
64 frames; relative processing times, NOT FM1 CPU percentages):
  Plain one-pole LP: ~0.48x Clean (no resonance/multimode behavior)
  Two cascaded poles: ~0.51x Clean (no resonance)
  Clean SVF: 1.00x
  MCO common SVF: ~1.29x
  CS-80: ~2.11x; SEM ~2.39x; Juno ~2.44x; Polysix ~26.45x.
Different features and cutoff/resonance laws mean this is not an equal-sound
comparison. Host rankings are not guaranteed on PI32v2. Start hardware testing
with Clean on both pads. Polysix is substantially heavier.

Project storage upgrades from format22 to format23 without increasing the
save-object size. Existing active settings, all pieces and pattern banks are
retained. Five unused Bass-era IDs now store Pad 2 filter settings; migration
initializes them to the new defaults. A former FM bypass initializes Pad 1 to
Clean; other former FM filter choices/settings transfer to Pad 1. Older
firmware does not understand the new project format. For snapshots, use the
included updated USB editor; the total parameter count stays 167.

Validation: target/package checks, filter reference/isolation (884736 samples),
page bindings, mixer placement, format22 migration and format23 save/load,
167-parameter editor metadata, Pad roles/movement and ASan/UBSan pass.
A 90-second full host render passed with two-note Pad 2 limit, no duplicate
note-ons, no notes held after Stop and bounded audio. Hardware CPU and
listening tests remain. No extra synth voices or effects buffers were added.

Target: app 306768 B; RAM 96980 / 98304; pool 234464 / 344064.
Flash genAMB-DUSK-TEST4.fwsc with your usual FWSC updater.

Build on Linux x86-64:
  export JIELI_TOOLCHAIN=/path/to/jieli-linux-toolchains-20260730.1
  sh build.sh
Output: app/build/amb-17.99.17.fwsc
Host: sh build.sh --host --release 17.99.17 --display-version 'DUSK TEST4'
Bundled fonts/minimal SDK packaging resources are under build-deps.
