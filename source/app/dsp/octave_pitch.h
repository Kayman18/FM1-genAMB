/* SPDX-License-Identifier: MPL-2.0
 * Shared octave bounds for the extended -4..+2 instrument controls.
 */
#ifndef GENAMB_OCTAVE_PITCH_H
#define GENAMB_OCTAVE_PITCH_H

/* Preserve the selected pitch class at the MIDI limits. In particular, Pad
 * octave -4 plus downward Oct Drift must not wrap a negative note into uint8_t.
 * Use the same pitch for the oscillator, emitted MIDI, and note display.
 */
static inline __attribute__((always_inline)) int en_pad_pitch(int note, int drift, int octave)
{
    int n = 48 + note % 12 + drift + 12 * octave;
    while (n < 0) n += 12;
    while (n > 127) n -= 12;
    return n;
}

/* The released Original loops fold into 36..103 after octave transposition.
 * Keep every -2..+2 setting identical. The two new choices lower the existing
 * -2 result by one/two octaves, so that legacy floor cannot undo the extension.
 */
static inline int en_original_octave_pitch(int note, int octave)
{
    int extra = octave < -2 ? octave + 2 : 0;
    int n = note + 12 * (octave - extra);
    while (n > 103) n -= 12;
    while (n < 36) n += 12;
    return n + 12 * extra;
}

#endif
