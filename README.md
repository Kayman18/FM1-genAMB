# genAMB FM-1 — 1.0 RC2

Custom firmware for the FM-1. 

It will start in PIANO mode (this mode will let you play Rings or the FM synth Via the Keybed. Depending on Which one you have selected), only the Pad synths will run. Turn the Algorithm knob to place it in scale mode and have the sequencers trigger notes.

*  2 Pad Synths (Both have different Characters Pad 1 - Vintage and Rhodes. Pad 2 - Strings and Glass. Pad 1 has a Chorus Effect, Pad 2 has a Stereo Width setting (cpu maxing out)
*  Rings port - Modes Kept - Modal, Sympathetic stings, and Strings the rest had to go for space and memory
*  6Voice FM synth, with 16 Presets - Koto, Harp, Brt Harp, Drop, Mallet, Bell, Rin, Bamboo, Furin, Marimba, Celeste, Kalimba, Xylophone, Glockenspiel, Warm Bass, Reso Bass.
*  3 Ported Sequencers from VCVrack: Hallucigenia, Proteus, UNIT-D
*  Burst Setting, to simulate Chords strums
*  Delay on Rings (too costly to add everywhere, may try to add it to FM later)
*  Global 4 Channel Mixer
*  10 Reverb Modes Inspired by the ones on the Valeton GP-05



## Downloads

- [Precompiled RC2 firmware](https://raw.githubusercontent.com/Kayman18/genAMB/main/firmware/genAMB-1.0-RC2-Hallucigenia-Port.fwsc)
- [Complete RC2 source archive](https://raw.githubusercontent.com/Kayman18/genAMB/main/source/genAMB-1.0-RC2-Complete-Source.zip)
- [Release notes and validation summary](https://github.com/Kayman18/genAMB/blob/main/docs/Release-Notes-1.0-RC2.md)
- [Build instructions](https://github.com/Kayman18/genAMB/blob/main/docs/BUILDING-1.0-RC2.md)
- [SHA-256 checksums](https://github.com/Kayman18/genAMB/blob/main/SHA256SUMS)

Extract the source ZIP before building. It includes the application, platform code, vendored DSP sources, original upstream Hallucigenia source, tests, and license notices. The compiler, SDK resources and fonts are external to this archive; see the build instructions.

## Attribution and licensing

Built on Charles Vestal's [fm1-amb](https://github.com/charlesvestal/fm1-amb) and [fm1-base](https://github.com/charlesvestal/fm1-base), with [felucca-bsp](https://github.com/hugelton/felucca-bsp) by Leo Kuroshita / Hügelton Instruments. Synthesis components include Mutable Instruments Rings/stmlib by Emilie Gillet and the [Zen-1](https://github.com/aceyprime/zen-1) six-operator FM engine and its msfa-derived core.

### Sequencers

- **Original** — original AMB-1 generative sequencing by [Charles Vestal](https://github.com/charlesvestal/fm1-amb), retained and extended for genAMB (MPL-2.0 provenance).
- **Hallucigenia / Darwinism** — ported from [Eater of Sheep's Extratone / XTRTN source](https://github.com/EaterOfSheep/Extratone/blob/main/src/Darwinism.cpp), GPL-3.0-or-later. RC2 replaces the independent Evolve implementation with this upstream-derived core.
- **Proteus** — adapted from [Seaside Modular Proteus](https://github.com/abluenautilus/SeasideModularVCV/blob/master/src/Proteus.cpp), by Seaside Modular contributors (MIT).
- **UNIT-D / UnitDistanceSeq** — adapted from [Animatek's UNIT-D](https://github.com/animatek/UZZ-VCV-RACK/blob/main/src/UnitDistanceSeq.cpp); the adaptation is GPL-3.0-or-later.

Original copyright and component license notices are retained in the complete source archive. See [LICENSING.md](https://github.com/Kayman18/genAMB/blob/main/LICENSING.md) for component provenance and combined-firmware licensing.
