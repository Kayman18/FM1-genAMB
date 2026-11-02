/* SPDX-License-Identifier: MIT */
#pragma once

/* Keep the original project IDs for Modal (0), Sympathetic (1), and String (2).
 * Removed models load as the closest retained string model: legacy FM (3),
 * String+Reverb (5), and StringSynth (6) become String; Sym Q (4) becomes
 * Sympathetic. Saved parameters retain their historical 0..6 validation span. */
#define RINGS_MODEL_COUNT 3

static inline int rings_model_normalize(int value)
{
    if (value < 0) value = 0;
    if (value > 6) value = 6;
    if (value == 4) return 1;
    return value > 2 ? 2 : value;
}

static inline int rings_model_index(int saved_id)
{
    return rings_model_normalize(saved_id);
}

static inline int rings_model_from_index(int ordinal)
{
    if (ordinal < 0) ordinal = 0;
    if (ordinal >= RINGS_MODEL_COUNT) ordinal = RINGS_MODEL_COUNT - 1;
    return ordinal;
}
