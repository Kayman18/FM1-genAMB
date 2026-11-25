/* SPDX-License-Identifier: GPL-3.0-only
 * genAMB Nature four-voice virtual analog instrument.
 */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void va_controls(const int16_t *params,const volatile float *targets);
#define FM_PORT_VOICES 4
#define FM_PORT_DEFAULT_VOICES 4

/* Audio owner only, after joining the worker. Gate zero sustains to note-off. */
void fm_port_init(void);
void fm_port_panic(void);
void fm_port_all_notes_off(void);
void fm_port_trigger(int midi_note, int velocity, uint32_t gate_samples);
void fm_port_note_off(int midi_note);
void fm_port_set_level(int value_0_100);
float fm_port_set_level_modulated(int base, float offset_percent);
void fm_port_set_polyphony(int voices_1_4);
void fm_port_set_tune(int cents);
void fm_port_render_add(float *left, float *right, float *reverb_send,
                        float send_amount, uint32_t n);

/* Lowering polyphony releases excess voices; their release tails can briefly
 * remain.
 */
extern volatile float fm_port_level;
extern volatile uint8_t fm_port_active_voices;

#ifdef __cplusplus
}
#endif
