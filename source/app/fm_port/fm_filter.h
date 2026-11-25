/* SPDX-License-Identifier: ISC
 * genAMB post-mix classic filter bank. Original embedded implementations;
 * character studies, NOT component-calibrated emulations of named hardware.
 */
#pragma once
#include <stdint.h>
enum { FM_FILTER_BYPASS, FM_FILTER_CLEAN, FM_FILTER_SEM,
       FM_FILTER_JUNO, FM_FILTER_POLYSIX, FM_FILTER_CS80, FM_FILTER_COUNT };
void fm_filter_init(void);
void fm_filter_set(int model, int type, int cutoff, int resonance, int wet,
                   float mod_cutoff, float mod_res);
float fm_filter_sample(float input);

typedef struct {
    float z[4], sv[2], high[2];
} fm_filter_history_t;

typedef struct {
    fm_filter_history_t history;
    float g, target_g, gh, target_gh, resonance, target_res, wet, target_wet;
    float blend, target_blend;
    uint8_t model, type, pending_model, pending_type;
    /* genAMB: set while the bank is fully dry and therefore skipped entirely.
     * On the first block that is not dry, the smoothed coefficients are
     * snapped to their targets, which is where they would have arrived
     * anyway after a few hundred samples of tracking nothing. */
    uint8_t dormant;
} fm_filter_state_t;
void classic_filter_init(fm_filter_state_t *fs);
void classic_filter_set(fm_filter_state_t *fs,int model,int type,int cutoff,int resonance,int wet,float mod_cutoff,float mod_res);
float classic_filter_sample(fm_filter_state_t *fs,float input);

void classic_filter_stereo_sample(fm_filter_state_t *fs, fm_filter_history_t *right,
                                  float *left_sample, float *right_sample);
/* Block entry. Bit-exact to calling classic_filter_stereo_sample once per
 * sample whenever the bank is doing anything at all; when the bank is fully
 * dry it does nothing, because ff_mix() returns the dry input unchanged for
 * every sample of such a block. */
void classic_filter_stereo_block(fm_filter_state_t *fs, fm_filter_history_t *right,
                                 float *left, float *right_buf, uint32_t n);
