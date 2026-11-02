/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include "rings_models.h"
#ifdef __cplusplus
extern "C" {
#endif

enum {
    RP_MODEL_MODAL = 0,
    RP_MODEL_SYMPATHETIC = 1,
    RP_MODEL_STRING = 2,
    RP_MODEL_FM = 3, /* Reserved legacy ID; normalizes to String. */
    RP_MODEL_SYMPATHETIC_QUANTIZED = 4, /* Reserved; normalizes to Sympathetic. */
    RP_MODEL_STRING_REVERB = 5, /* Reserved; normalizes to String. */
    RP_MODEL_STRING_SYNTH = 6, /* Reserved; normalizes to String. */
    RP_MODEL_COUNT = 7 /* Stored ID span; RINGS_MODEL_COUNT is selectable count. */
};

enum {
    RP_DEST_OFF = 0,
    RP_DEST_STRUCTURE,
    RP_DEST_BRIGHTNESS,
    RP_DEST_DAMPING,
    RP_DEST_POSITION,
    RP_DEST_PITCH,
    RP_DEST_MODEL,
    RP_DEST_LEVEL,
    RP_DEST_EXCITER = 14,
    RP_DEST_COUNT = 15
};

void cloud_reverb_port_init(void);
void cloud_reverb_port_set(float size_0_1, float damping_0_1, float mod_0_1, float space_0_1);
void cloud_reverb_port_process(float *mono_inout, float *wet_right, uint32_t n);

void rings_port_init(void);
void rings_port_panic(void);
void rings_port_trigger(int midi_note, int velocity);
void rings_port_set_tune(int cents);
void rings_port_set_level(int value_0_100);
void rings_port_set_model(int model);
void rings_port_set_polyphony(int voices_1_4);
void rings_port_set_structure(int value_0_100);
void rings_port_set_brightness(int value_0_100);
void rings_port_set_damping(int value_0_100);
void rings_port_set_position(int value_0_100);
void rings_port_set_string_fx(int fx_0_5); /* Legacy project field; now a no-op. */
void rings_port_set_exciter(int value);
void rings_port_set_delay(int time_0_100, int feedback_0_85, int mix_0_100, int repeats_0_8);
void rings_port_set_ambient(int diffusion, int motion, int reverb_send);
void rings_port_set_modulation(
    float src7, int dest7, int depth7,
    float src8, int dest8, int depth8,
    float src9, int dest9, int depth9);
void rings_port_render_add(float *left, float *right, float *reverb_send, float send_amount, uint32_t n);

#ifdef __cplusplus
}
#endif
