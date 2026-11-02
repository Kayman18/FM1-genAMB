/* SPDX-License-Identifier: GPL-3.0-only
 * genAMB: a separate six-voice FM layer using ZEN-1's compact msfa port.
 * No heap, no large patch bank, no duplicated effects or platform code.
 */
#include "fm_port.h"
#include "fm_filter.c"
#include <string.h>
#include "../vendor/zen_fm/fm6_core.c"
#include "../vendor/zen_fm/fm6_patch.c"
#include "fm_factory.h"

#if defined(PLAT_HOST) || defined(EN_HOST)
#define FM_POOL
#else
#define FM_POOL __attribute__((section(".pool")))
#endif

typedef struct {
    fm6_note_t core;
    uint32_t gate, age;
    int32_t logfreq;
    uint8_t note, patch, timed, reserved;
} fm_voice_t;

/* Keep the six voices and the small per-preset LFOs in the existing pool.
 * A voice keeps its preset index, so changing the selector leaves its tail
 * intact. All voices on one preset share its LFO, as ZEN-1's parts do.
 */
static struct {
    fm_voice_t voice[FM_PORT_VOICES];
    fm6_lfo_t lfo[FM_PORT_PATCHES];
} state FM_POOL;
/* The former 1832-byte allowance plus the 576 bytes reclaimed by removing
 * Rings' internal FMVoice array gives 2408 bytes. Six voices and sixteen
 * LFOs use 2256 bytes, leaving 152 of that allowance unused. The whole-device
 * pool reserve and the separately sized UI canvas remain build-time checks.
 */
typedef char fm_pool_budget[(sizeof state <= 4000u) ? 1 : -1];

static int32_t temporary[FM6_N];
static float cached[FM6_N];
static uint32_t serial;
static int32_t tune;
static uint8_t position, current_patch, polyphony;
static float target_gain, gain;
volatile float fm_port_level;
volatile uint8_t fm_port_active_voices;
static const int32_t DT0[6] = {0, 0, 0, 0, 0, 0};

static int bounded(int n, int lo, int hi)
{
    return n < lo ? lo : n > hi ? hi : n;
}

void fm_port_init(void)
{
    unsigned k;
    memset(&state, 0, sizeof state);
    memset(cached, 0, sizeof cached);
    memset(temporary, 0, sizeof temporary);
    memset(fm6_bus, 0, sizeof fm6_bus);
    for (k = 0; k < FM_PORT_PATCHES; ++k)
        fm6_lfo_reset(&state.lfo[k], FM_FACTORY[k]);
    serial = 0;
    tune = 0;
    position = FM6_N;
    current_patch = 0;
    polyphony = FM_PORT_DEFAULT_VOICES;
    target_gain = gain = fm_port_level = 0.0f;
    fm_port_active_voices = 0;
    fm_filter_init();
}

void fm_port_panic(void)
{
    /* An explicit panic intentionally discards queued sound, including cache. */
    memset(state.voice, 0, sizeof state.voice);
    memset(cached, 0, sizeof cached);
    position = FM6_N;
    fm_port_level = 0.0f;
    fm_port_active_voices = 0;
}

static void release(fm_voice_t *v)
{
    if (v->core.live && v->core.down)
        fm6_note_key(&v->core, FM_FACTORY[v->patch], 0);
    v->timed = 0;
}

void fm_port_all_notes_off(void)
{
    unsigned k;
    for (k = 0; k < FM_PORT_VOICES; ++k)
        release(&state.voice[k]);
}

void fm_port_note_off(int midi_note)
{
    unsigned k;
    for (k = 0; k < FM_PORT_VOICES; ++k)
        if ((int)state.voice[k].note == midi_note)
            release(&state.voice[k]);
}

void fm_port_set_patch(int index)
{
    current_patch = (uint8_t)bounded(index, 0, FM_PORT_PATCHES - 1);
}

const char *fm_port_patch_name(int index)
{
    return (const char *)(FM_FACTORY[bounded(index, 0, FM_PORT_PATCHES - 1)] + FP_NAME);
}

void fm_port_set_level(int value)
{
    value = bounded(value, 0, 100);
    target_gain = (float)(value * value) * 0.0001f;
}

void fm_port_set_polyphony(int voices)
{
    unsigned k;
    polyphony = (uint8_t)bounded(voices, 1, FM_PORT_VOICES);
    for (k = polyphony; k < FM_PORT_VOICES; ++k)
        release(&state.voice[k]);
}

void fm_port_set_tune(int cents)
{
    tune = (int32_t)((float)bounded(cents, -1200, 1200) * 13981.0133f);
}

void fm_port_trigger(int midi_note, int velocity, uint32_t gate_samples)
{
    unsigned k, slot = 0;
    uint32_t greatest_age = 0;
    int found = 0, note, fresh;
    fm_voice_t *v;
    const uint8_t *p;
    if (midi_note < 0 || midi_note > 127)
        return;
    if (velocity <= 0) {
        fm_port_note_off(midi_note);
        return;
    }
    /* Re-strike an already-held identical note; otherwise free, released-oldest,
     * then oldest-held. Subtraction makes age ordering safe across wraparound.
     */
    for (k = 0; k < polyphony; ++k)
        if (state.voice[k].core.live && state.voice[k].core.down &&
            state.voice[k].note == midi_note && state.voice[k].patch == current_patch) {
            slot = k;
            found = 1;
            break;
        }
    if (!found)
        for (k = 0; k < polyphony; ++k)
            if (!state.voice[k].core.live) {
                slot = k;
                found = 1;
                break;
            }
    if (!found) {
        for (k = 0; k < polyphony; ++k)
            if (!state.voice[k].core.down &&
                (!found || serial - state.voice[k].age >= greatest_age)) {
                greatest_age = serial - state.voice[k].age;
                slot = k;
                found = 1;
            }
    }
    if (!found)
        for (k = 0; k < polyphony; ++k)
            if (serial - state.voice[k].age >= greatest_age) {
                greatest_age = serial - state.voice[k].age;
                slot = k;
            }
    v = &state.voice[slot];
    p = FM_FACTORY[current_patch];
    note = bounded(midi_note + (int)p[FP_TRNSP] - 24, 0, 127);
    fresh = !v->core.live;
    fm6_note_init(&v->core, p, note, bounded(velocity, 1, 127), fresh);
    v->note = (uint8_t)midi_note;
    v->patch = current_patch;
    v->gate = gate_samples;
    v->timed = gate_samples != 0u;
    v->age = ++serial;
    v->logfreq = fm6_logfreq_cents((float)note * 100.0f);
    fm6_lfo_key(&state.lfo[current_patch]);
}

static void make_block(void)
{
    unsigned k, j, active = 0;
    int32_t lfo_value[FM_PORT_PATCHES], lfo_delay[FM_PORT_PATCHES];
    for (j = 0; j < FM6_N; ++j)
        cached[j] = 0.0f;
    for (k = 0; k < FM_PORT_PATCHES; ++k) {
        lfo_value[k] = fm6_lfo_sample(&state.lfo[k]);
        lfo_delay[k] = fm6_lfo_delay(&state.lfo[k]);
    }
    for (k = 0; k < FM_PORT_VOICES; ++k) {
        fm_voice_t *v = &state.voice[k];
        const uint8_t *p;
        if (!v->core.live)
            continue;
        p = FM_FACTORY[v->patch];
        if (v->timed && !v->gate)
            release(v);
        if (fm6_note_compute(&v->core, p, temporary, lfo_value[v->patch],
                            lfo_delay[v->patch], v->logfreq + tune,
                            p[FP_ALG], p[FP_FB], DT0, 0)) {
            for (j = 0; j < FM6_N; ++j) {
                int32_t sample = temporary[j] - v->core.dc;
                v->core.dc += sample >> 9;
                cached[j] += (float)sample * (0.12f / 16777216.0f);
            }
        }
        if (v->timed)
            v->gate = v->gate > FM6_N ? v->gate - FM6_N : 0;
        if (fm6_note_done(&v->core, p, p[FP_ALG]))
            v->core.live = 0;
        active += v->core.live != 0;
    }
    fm_port_active_voices = (uint8_t)active;
    position = 0;
}

void fm_port_render_add(float *left, float *right, float *reverb_send,
                        float send_amount, uint32_t n)
{
    uint32_t i;
    float meter = fm_port_level;
    send_amount = send_amount < 0.0f ? 0.0f : send_amount > 1.0f ? 1.0f : send_amount;
    for (i = 0; i < n; ++i) {
        float sample, magnitude;
        if (position >= FM6_N)
            make_block();
        gain += (target_gain - gain) * 0.004f;
        if (target_gain == 0.0f && gain < 0.000001f)
            gain = 0.0f;
        sample = fm_filter_sample(cached[position++] * gain);
        left[i] += sample;
        right[i] += sample;
        if (reverb_send)
            reverb_send[i] += sample * send_amount;
        magnitude = sample < 0.0f ? -sample : sample;
        meter *= 0.9995f;
        if (magnitude > meter)
            meter = magnitude;
    }
    fm_port_level = meter;
}
