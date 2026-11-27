/* SPDX-License-Identifier: MIT
 * Adapter between AMB-1/fm1-base and the upstream Mutable Instruments Rings DSP.
 */
#include "rings_port.h"
#include "rings/dsp/dsp.h"
#include "rings/dsp/part.h"
#include "rings/dsp/patch.h"
#include "rings/dsp/performance_state.h"
#include "rings/dsp/fx/fx_engine.h"
#include "monumatic_reverb_port.h"

extern "C" float fabsf(float x) { return x < 0.0f ? -x : x; }
extern "C" float sqrtf(float x) {
    if (x <= 0.0f) return 0.0f;
    float y = x > 1.0f ? x : 1.0f;
    for (int i = 0; i < 6; ++i) y = 0.5f * (y + x / y);
    return y;
}

#if defined(PLAT_HOST) || defined(EN_HOST)
#define RP_POOL
#else
#define RP_POOL __attribute__((section(".pool")))
#endif

namespace {
static rings::Part g_part RP_POOL;
static rings::Patch g_base_patch;
static rings::Patch g_patch;
static rings::PerformanceState g_state;
static int g_model = RP_MODEL_MODAL;
static int g_effective_model = RP_MODEL_MODAL;
static int g_requested_model = RP_MODEL_MODAL;
static float g_model_offset_latched = 0.0f;
static int g_model_fade_left = 0;
static int g_model_fade_total = 0;

static int g_polyphony = 4;
static float g_base_level = 0.65f;
static float g_level = 0.65f;
static float g_pitch_mod = 0.0f;
static float g_tune = 0.0f;
static float g_base_exciter = 0.55f;
static float g_exciter = 0.55f;
static int g_pending_note = 60;
static int g_pending_velocity = 100;
static int g_pending_strum = 0;
static int g_muted = 0;

static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static float norm100(int v) { if (v < 0) v = 0; if (v > 100) v = 100; return (float)v * 0.01f; }

static void apply_model() {
    g_effective_model = rings_model_normalize(g_effective_model);
    g_part.set_model((rings::ResonatorModel)g_effective_model);
    g_part.set_polyphony(g_polyphony);
}
static void mod_one(float x, int dest, int depth, float *model_sum) {
    float d = x * clampf((float)depth * 0.01f, -1.0f, 1.0f);
    switch (dest) {
    case RP_DEST_STRUCTURE:  g_patch.structure  = clampf(g_patch.structure  + 0.45f*d, 0.0f, 1.0f); break;
    case RP_DEST_BRIGHTNESS: g_patch.brightness = clampf(g_patch.brightness + 0.45f*d, 0.0f, 1.0f); break;
    case RP_DEST_DAMPING:    g_patch.damping    = clampf(g_patch.damping    + 0.45f*d, 0.0f, 1.0f); break;
    case RP_DEST_POSITION:   g_patch.position   = clampf(g_patch.position   + 0.45f*d, 0.0f, 1.0f); break;
    case RP_DEST_PITCH:      g_pitch_mod += d * 2.0f; break;
    case RP_DEST_MODEL:      *model_sum += d * (RINGS_MODEL_COUNT - 1); break;
    case RP_DEST_LEVEL:      g_level = clampf(g_level + 0.45f*d, 0.0f, 1.0f); break;
    case RP_DEST_EXCITER:    g_exciter = clampf(g_exciter + 0.50f*d, 0.0f, 1.0f); break;
    default: break;
    }
}
}

extern "C" void rings_port_init(void) {
    g_part.Init(NULL);
    g_base_patch.structure = 0.45f;
    g_base_patch.brightness = 0.55f;
    g_base_patch.damping = 0.65f;
    g_base_patch.position = 0.35f;
    g_patch = g_base_patch;
    g_state.strum = false;
    g_state.internal_exciter = true;
    g_state.internal_strum = false;
    g_state.internal_note = false;
    g_state.tonic = 0.0f;
    g_tune = 0.0f;
    g_state.note = 60.0f;
    g_state.fm = 0.0f;
    g_state.chord = 10;
    g_effective_model = g_model;
    g_requested_model = g_model;
    apply_model();
}
extern "C" void rings_port_panic(void) { g_pending_strum = 0; g_muted = 1; }
extern "C" void rings_port_trigger(int note, int velocity) {
    if (note < 0) note = 0; if (note > 127) note = 127;
    if (velocity < 1) velocity = 1; if (velocity > 127) velocity = 127;
    /* Model modulation is intentionally deferred until a new strike. Changing
     * Rings' resonator model under an already-ringing tail can create a hard
     * discontinuity. Commit the requested model here, immediately before the
     * new excitation, and fade the new output in over ~30 ms. */
    if (g_requested_model != g_effective_model) {
        g_effective_model = g_requested_model;
        apply_model();
        g_model_fade_total = 1323; /* ~30 ms at 44.1 kHz */
        g_model_fade_left = g_model_fade_total;
    }
    g_pending_note = note; g_pending_velocity = velocity; g_pending_strum = 1; g_muted = 0;
}
extern "C" void rings_port_set_tune(int cents) {
    if (cents < -50) cents = -50; if (cents > 50) cents = 50;
    g_tune = (float)cents * 0.01f;
}
extern "C" void rings_port_set_level(int v) { float x = norm100(v); g_base_level = x*x; g_level = g_base_level; }
extern "C" void rings_port_set_model(int v) {
    v = rings_model_normalize(v);
    /* Manual model changes remain immediate; only modulation is deferred. */
    g_model=v; g_effective_model=v; g_requested_model=v; g_model_offset_latched=0.0f; apply_model();
}
extern "C" void rings_port_set_polyphony(int v) { if (v<1) v=1; if (v>4) v=4; g_polyphony=v; apply_model(); }
extern "C" void rings_port_set_structure(int v) { g_base_patch.structure = norm100(v); }
extern "C" void rings_port_set_brightness(int v) { g_base_patch.brightness = norm100(v); }
extern "C" void rings_port_set_damping(int v) { g_base_patch.damping = norm100(v); }
extern "C" void rings_port_set_position(int v) { g_base_patch.position = norm100(v); }
extern "C" void rings_port_set_string_fx(int v) { (void)v; }
extern "C" void rings_port_set_exciter(int v) { g_base_exciter = norm100(v); g_exciter = g_base_exciter; }
extern "C" void rings_port_set_modulation(float s7,int d7,int a7,float s8,int d8,int a8,float s9,int d9,int a9) {
    const rings_mod_route_t routes[]={{s7,d7,a7},{s8,d8,a8},{s9,d9,a9}};
    rings_port_mod_routes(routes,3);
}
extern "C" void rings_port_mod_routes(const rings_mod_route_t *routes, int count) {
    float model_sum = 0.0f;
    g_patch = g_base_patch; g_pitch_mod = 0.0f; g_level = g_base_level; g_exciter = g_base_exciter;
    for (int i=0;i<count;i++) mod_one(routes[i].value,routes[i].destination,routes[i].depth,&model_sum);
    /* Quantized Model modulation now spans the three retained models, with the
     * original hysteresis and next-strike commit. Moderate depths must be able
     * to move the model, but slow random motion must not chatter at boundaries. */
    while (model_sum > g_model_offset_latched + 0.58f) g_model_offset_latched += 1.0f;
    while (model_sum < g_model_offset_latched - 0.58f) g_model_offset_latched -= 1.0f;
    {
        int step = (int)g_model_offset_latched;
        int m = rings_model_from_index(rings_model_index(g_model) + step);
        /* Do not switch the resonator here. Slow modulation only chooses the
         * model that the NEXT strike will use. This avoids tearing an active
         * resonant tail apart mid-note. */
        g_requested_model = m;
    }
}
extern "C" void rings_port_render_add(float *left, float *right, float *reverb_send, float send_amount, uint32_t n) {
    float out[rings::kMaxBlockSize];
    float aux[rings::kMaxBlockSize];
    while (n) {
        uint32_t k = n > rings::kMaxBlockSize ? rings::kMaxBlockSize : n;
        g_state.note = (float)g_pending_note + g_pitch_mod + g_tune;
        g_state.fm = 0.0f;
        rings::Patch render_patch = g_patch;
        render_patch.brightness = clampf(render_patch.brightness + (g_exciter - 0.5f) * 0.34f, 0.0f, 1.0f);
        g_state.strum = g_pending_strum != 0;
        if (g_state.strum) g_pending_strum = 0;
        /* genAMB: the mix below scales everything by g_level, so at level zero
         * - and while muted, where the mix is skipped outright - every sample
         * this resonator produces is multiplied by zero before it reaches the
         * buses. Running four voices of modal filters to throw them away was a
         * sixth of the whole engine. The pending strum is still consumed here,
         * so strums cannot pile up and fire together when the level returns;
         * what holds still is the resonator's own ringing, which is what
         * "silent" means. Rings has its own RNG, so no other layer moves. */
        const bool audible = !g_muted && g_level != 0.0f;
        if (audible) {
            g_part.Process(g_state, render_patch, NULL, out, aux, k);
        }
        g_state.strum = false;
        if (audible) {
            float vel=(float)g_pending_velocity*(1.0f/127.0f);
            float gain=g_level*(0.35f+0.65f*vel)*(0.72f+0.56f*g_exciter);
            for(uint32_t i=0;i<k;i++){
                float fade = 1.0f;
                if (g_model_fade_left > 0 && g_model_fade_total > 0) {
                    fade = 1.0f - (float)g_model_fade_left / (float)g_model_fade_total;
                    if (--g_model_fade_left < 0) g_model_fade_left = 0;
                }
                float lo=out[i]*gain*fade, ro=aux[i]*gain*fade;
                left[i] += lo;
                right[i] += ro;
                if (reverb_send) reverb_send[i] += 0.5f * (lo + ro) * send_amount;
            }
        }
        left+=k; right+=k;
        /* rings::Part renders in kMaxBlockSize chunks. The old adapter advanced
         * the dry L/R pointers but not the reverb-send pointer, so every chunk
         * was summed repeatedly into the start of the same send buffer. That
         * made Rings alone create huge send spikes and audible breakup. */
        if (reverb_send) reverb_send += k;
        n-=k;
    }
}
