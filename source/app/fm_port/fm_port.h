/* SPDX-License-Identifier: GPL-3.0-only
 * genAMB's compact adapter for ZEN-1 / Felucca's msfa six-operator FM core.
 */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define FM_PORT_PATCHES 16
#define FM_PORT_VOICES 6
#define FM_PORT_DEFAULT_VOICES 4

/* Call from the audio owner, after joining any outstanding DSP worker.
 * Rendering uses 32-sample blocks. Events start on the next block boundary,
 * at most 31 samples later; cached audio is preserved across control changes.
 * A zero gate sustains until note_off/all_notes_off. Other gates specify
 * sample counts, rounded up to a 32-sample boundary, followed by patch release.
 */
void fm_port_init(void);
void fm_port_panic(void);
void fm_port_all_notes_off(void);
void fm_port_trigger(int midi_note, int velocity, uint32_t gate_samples);
void fm_port_note_off(int midi_note);
void fm_port_set_patch(int index);
const char *fm_port_patch_name(int index);
void fm_port_set_level(int value_0_100);
void fm_port_set_polyphony(int voices_1_6);
void fm_port_set_tune(int cents);
void fm_port_render_add(float *left, float *right, float *reverb_send,
                        float send_amount, uint32_t n);

/* Lowering polyphony releases excess voices; their release tails can briefly
 * remain. Patch changes retain sounding notes on their original patch.
 */
extern volatile float fm_port_level;
extern volatile uint8_t fm_port_active_voices;

#ifdef __cplusplus
}
#endif
