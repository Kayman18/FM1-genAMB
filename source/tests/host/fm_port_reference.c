/* SPDX-License-Identifier: GPL-3.0-only
 * PCM fingerprint for comparison with pinned upstream core source. Override
 * FM6_SOURCE with its absolute filename to compare the same exact render.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifndef FM6_SOURCE
#define FM6_SOURCE "../../app/vendor/zen_fm/fm6_core.c"
#endif
#include FM6_SOURCE
#include "../../app/vendor/zen_fm/fm6_patch.c"
#define ZN_PATCH_DATA
#include "../../app/vendor/zen_fm/zen_patches_zen9.h"

int main(int argc, char **argv)
{
    static const int pitches[] = {24, 60, 90};
    static const int32_t zero[6] = {0, 0, 0, 0, 0, 0};
    fm6_note_t voice;
    fm6_lfo_t lfo;
    int32_t out[32];
    uint8_t p[156];
    uint32_t hash = 2166136261u, samples = 0;
    unsigned patch, pitch, block, j, b;
    (void)argv;
    for (patch = 0; patch < 9; ++patch)
        for (pitch = 0; pitch < 3; ++pitch) {
            memset(&voice, 0, sizeof voice);
            memset(&lfo, 0, sizeof lfo);
            fm6_unpack(ZN_PATCH[patch], p);
            fm6_lfo_reset(&lfo, p);
            fm6_lfo_key(&lfo);
            fm6_note_init(&voice, p, pitches[pitch], 110, 1);
            for (block = 0; block < 256; ++block) {
                int32_t lv = fm6_lfo_sample(&lfo), ld = fm6_lfo_delay(&lfo);
                if (block == 128)
                    fm6_note_key(&voice, p, 0);
                if (!fm6_note_compute(&voice, p, out, lv, ld,
                                     fm6_logfreq_cents((float)pitches[pitch] * 100.0f),
                                     p[FP_ALG], p[FP_FB], zero, 0))
                    memset(out, 0, sizeof out);
                if (argc > 1)
                    fwrite(out, sizeof out, 1, stdout);
                for (j = 0; j < 32; ++j)
                    for (b = 0; b < 4; ++b) {
                        hash ^= ((uint32_t)out[j] >> (8 * b)) & 255u;
                        hash *= 16777619u;
                    }
                samples += 32;
            }
        }
    if (argc == 1)
        printf("%08x %u samples\n", hash, samples);
    return 0;
}
