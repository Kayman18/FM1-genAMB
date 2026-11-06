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
