/* SPDX-License-Identifier: MPL-2.0 */
#pragma once
#include <stdint.h>
#include "animal_wave_layout.h"
void en_nature_mount(const uint8_t *p, unsigned capacity);
unsigned en_nature_calls(void);
enum { NATURE_RAIN_OFF, NATURE_FADE_IN, NATURE_WET, NATURE_FADE_OUT, NATURE_DRY };
/* Audio publishes a single aligned snapshot after each rendered block. */
extern volatile uint32_t nature_status;
#define NATURE_WATER_ON(status) ((status) & 1u)
#define NATURE_RAIN_PHASE(status) (((status) >> 1) & 7u)
#define NATURE_BIRDS(status) (((status) >> 4) & 3u)
