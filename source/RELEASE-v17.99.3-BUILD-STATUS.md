# genAMB v17.99.3 — FM-1 device firmware

**BUILD SUCCESSFUL — FLASHABLE TEST FIRMWARE**
Generated with uploaded `jieli-linux-toolchains-20260730.1.tar.xz` (PI32v2 Clang 4.0.1), recovered verified AC79 SDK boot/configuration files and actual genAMB v17.99.3 source.

Firmware: `genAMB-v17.99.3-FM1.fwsc`
Package: 610,108 bytes
Firmware SHA-256: 53692e236920547fefca31d0af61c7362968635937d5df2fa309bd3fd50b8550
FM-1 package identity: FM-1_7179903
Application code image: 268,520 bytes
Normal RAM: 95,044 / 98,304 bytes (3,260 bytes remaining)
Audio pool: 262,304 / 344,064 bytes (81,760 bytes remaining)

## Included changes since v17.99.2
- FM up to six voices; all 16 original v17.99.2 factory patch slots retained, and FM Original/Evolve independent sequence settings.
- Rings limited to Modal, Sympathetic, and String, with project migration mapping removed Rings models to surviving models.
- PAD1/PAD2/Rings/FM octave controls extend to -4.
- PAD1/PAD2/Rings/FM main controls and MOD LFO show two-by-two layouts.
- Instrument names removed from VU meter cards, and PAD colour shown in place of the old pad VU bars.
- Rings Tone/Exciter/Delay/Delay Ambient omit the duplicate mixer section and use enlarged controls.
- Main reverb Mix is a parallel return, leaving dry instrument volume at full with a zero send.
- New one-quarter and quadruple Evolve rates, independent for Rings and FM, preserved from v17.99.2.
- **Restored v17.99.1 C2 fault fix:** two audio-half hang watchdog (~11.61ms), distinct timeout/mailbox/stack fault codes, and sticky diagnostics. This worker was missing from the initially recovered source, and was recovered byte-for-byte from v17.99.1 source before compiling.

## Validation
PASS: real PI32v2 target compiler and link, image RAM/register/instruction gates.
PASS: exact .fwsc package identity, header/table/payload CRCs, loader, decrypted application, padding, length.
PASS: 41 FM adapter checks (normal and ASan/UBSan), all 16 preset outputs and 1-6 voices.
PASS: FM sequencer test, all five rate settings, chord/scale mapping and gate/steal behavior.
PASS: 102 worker tests (normal and ASan/UBSan), fault classification, 4.5ms completion, hang/mailbox/stack checks, timer wrap.
PASS: format-18 save/reboot, extended octaves, patch slot 16, voice count 6, format-17 migration.
PASS: mixed application host stress and host compilation.
KNOWN: older all-in-one tests/run_tests.sh contains obsolete parameter/page/preset expectations; it reports failures. Standalone octave/pending tests reference missing historical tests/rates/evolve_rate_test.c and cannot compile. Focused replacement tests listed above pass. Do not represent the full legacy suite as passing.

## Physical unit validation outstanding
The firmware has NOT been flashed or played on a physical FM-1 during this session. On the unit, start with four FM voices, then try six while running Rings and both pads. Watch GLO -> Performance / Worker and retest the FM preset/voice changes that previously triggered C2. Preserve a v17.99.2 firmware rollback copy. If C2 returns, record the full Worker fault reason.

## Build environment / reproducibility
The source package excludes third-party TrueType font files, compiler binaries and SDK blobs. Follow BUILD-FM1-LINUX.sh and BUILDING.md, providing the JieLi toolchain and AC79 SDK separately. The source archive includes the restored worker source and its test.
