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
/* Rings delay diffusion all-pass helper. This is independent of the removed
 * Clouds global reverb and is still required by ring_delay_process(). */
template<int N> struct AmbientAP {
  float b[N]; int pos;
  void Clear() { for (int i=0;i<N;++i) b[i]=0.0f; pos=0; }
  float Process(float x, float g) {
    float z=b[pos]; float y=z-g*x; b[pos]=x+g*y;
    if (++pos==N) pos=0; return y;
  }
};

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

/* Rings-only mini delay. v17.12 stores the delay at 22.05 kHz / 16-bit
 * instead of 11.025 kHz, which doubles the repeat bandwidth and removes much
 * of the metallic low-rate character while preserving the full 1.5-second
 * range. The buffer is split between normal RAM and .pool so it still fits
 * comfortably on the FM-1. */
#define RING_DELAY_SR 22050.0f
#define RING_DELAY_MAX_MS 1500
#define RING_DELAY_N 33088
#define RING_DELAY_RAM_N 26000
#define RING_DELAY_POOL_N (RING_DELAY_N - RING_DELAY_RAM_N)
static int16_t g_delay_buf_ram[RING_DELAY_RAM_N];
static int16_t g_delay_buf_pool[RING_DELAY_POOL_N] RP_POOL;

static inline int16_t delay_get(int i)
{
    return i < RING_DELAY_RAM_N ? g_delay_buf_ram[i] : g_delay_buf_pool[i - RING_DELAY_RAM_N];
}
static inline void delay_put(int i, int16_t v)
{
    if (i < RING_DELAY_RAM_N) g_delay_buf_ram[i] = v;
    else g_delay_buf_pool[i - RING_DELAY_RAM_N] = v;
}
static AmbientAP<127> delay_ap_l RP_POOL;
static AmbientAP<191> delay_ap_r RP_POOL;
static float g_diff=0.55f, g_motion=0.3f, g_delay_rev=0.65f;
static float g_wow=0.0f, g_wow_step=0.00001179f;
static float g_feedback_diff=0.0f, g_delay_hp=0.0f;
static int g_delay_w = 0;
static int g_delay_phase = 0;
static float g_delay_acc = 0.0f;
static float g_delay_in_lp = 0.0f;
static float g_delay_time = 0.333333f;   /* normalized control; old range retained through 80 */
static float g_delay_fb = 0.35f;
static float g_delay_mix = 0.0f;
static float g_delay_wet_gain = 0.0f;  /* cached additive wet return gain */
static float g_delay_fb_lp = 0.0f;
static float g_delay_qerr = 0.0f;
static int g_delay_repeats = 0;          /* 0 = unlimited feedback repeats */
static int g_delay_repeat_cycle = 0;     /* completed delay-length cycles since latest Rings strike */
static int g_delay_cycle_pos = 0;        /* storage-rate samples into the current delay cycle */

static inline float delay_read(float d)
{
    int k = (int)d;
    float f = d - (float)k;
    int i0 = g_delay_w - k;
    int i1 = i0 - 1;
    while (i0 < 0) i0 += RING_DELAY_N;
    while (i1 < 0) i1 += RING_DELAY_N;
    return ((float)delay_get(i0) + ((float)delay_get(i1) - (float)delay_get(i0)) * f) * (1.0f / 32767.0f);
}

static inline int16_t delay_s16(float x)
{
    int q;
    x += 0.85f * g_delay_qerr;             /* first-order quantization error feedback */
    if (x > 0.999969f) x = 0.999969f;
    if (x < -0.999969f) x = -0.999969f;
    q = (int)(x * 32767.0f + (x >= 0.0f ? 0.5f : -0.5f));
    g_delay_qerr = x - (float)q * (1.0f / 32767.0f);
    return (int16_t)q;
}

static inline void ring_delay_process(float in_l, float in_r, float *out_l, float *out_r)
{
    if (g_delay_mix <= 0.00001f) {
        /* True bypass at Mix 0: keep the current Rings sound and nearly zero CPU
         * cost. Clear the dormant buffer incrementally so re-enabling the delay
         * never resurrects an old frozen echo. */
        delay_ap_l.Process(0.0f, 0.6f); delay_ap_r.Process(0.0f, 0.6f);
        *out_l = in_l; *out_r = in_r;
        if (++g_delay_phase >= 2) {
            g_delay_phase = 0; g_delay_acc = 0.0f; g_delay_in_lp = 0.0f; g_delay_fb_lp = 0.0f; g_delay_qerr = 0.0f; g_feedback_diff = g_delay_hp = 0.0f; g_delay_repeat_cycle = 0; g_delay_cycle_pos = 0;
            delay_put(g_delay_w, 0);
            if (++g_delay_w >= RING_DELAY_N) g_delay_w = 0;
        }
        return;
    }
    /* Preserve the old 20..~644 ms mapping through control value 80, then
     * spend the top 20% of the knob on the extended range up to 1.5 s.
     * Existing moderate delay settings therefore keep essentially the same time. */
    const float ms = g_delay_time <= 0.8f
        ? 20.0f + 780.0f * g_delay_time
        : 644.0f + 4280.0f * (g_delay_time - 0.8f);
    const float d = ms * (RING_DELAY_SR * 0.001f);
    g_wow += g_wow_step;
    if (g_wow > 1.0f) { g_wow=1.0f; g_wow_step=-g_wow_step; }
    if (g_wow < -1.0f) { g_wow=-1.0f; g_wow_step=-g_wow_step; }
    const float wobble = (g_wow * (1.5f - 0.5f*g_wow*g_wow)) * 12.0f * g_motion;
    const float dr = d * 0.97f - wobble;
    const float phase = 0.5f * (float)g_delay_phase;
    float dl = delay_read(d + wobble - phase);
    float rr = delay_read(dr - phase);
    float diffuse_l=delay_ap_l.Process(dl, 0.6f);
    float diffuse_r=delay_ap_r.Process(rr, 0.6f);
    dl += g_diff * (diffuse_l-dl);
    rr += g_diff * (diffuse_r-rr);
    g_feedback_diff=0.5f*(dl+rr);
    /* v17.17: make the Rings delay read as an effect send rather than a dry/wet
     * crossfade. Dry Rings now stays at unity and the Mix knob adds the delayed
     * return on top. A square-root control curve makes the lower half of the
     * knob much more useful (Mix 50 is already close to a unity wet return),
     * while the 1.35 gain gives the wet path ~35% more presence at maximum.
     * Feedback and repeat-count behavior are intentionally unchanged. */
    *out_l = in_l + dl * g_delay_wet_gain;
    *out_r = in_r + rr * g_delay_wet_gain;

    {
        float mono = 0.5f * (in_l + in_r);
        g_delay_in_lp += 0.48f * (mono - g_delay_in_lp);
        /* Correct 2:1 decimation gain: two half-weight samples average to the
         * original amplitude. The v17.11 4:1 path used four half-weight samples
         * and therefore drove the 16-bit delay memory about 2x too hard. */
        g_delay_acc += 0.5f * g_delay_in_lp;
    }
    if (++g_delay_phase >= 2) {
        float fb = g_feedback_diff;
        float wr;
        int cycle_len = (int)(d + 0.5f);
        int feedback_allowed;
        if (cycle_len < 1) cycle_len = 1;
        g_delay_phase = 0;
        g_delay_fb_lp += 0.32f * (fb - g_delay_fb_lp);  /* darker repeats */
        /* Repeats counts the overall delay stream. 0 preserves the legacy
         * feedback-only behavior. For N finite repeats, feedback is written
         * during the first N-1 delay cycles; the original buffer contents
         * produce repeat #1 without requiring feedback. A fresh Rings strike
         * restarts the count. */
        feedback_allowed = g_delay_repeats == 0 || g_delay_repeat_cycle < g_delay_repeats - 1;
        g_delay_hp += 0.003f * (g_delay_fb_lp - g_delay_hp);
        wr = g_delay_acc + (feedback_allowed ? g_delay_fb * (g_delay_fb_lp - g_delay_hp) : 0.0f);
        g_delay_acc = 0.0f;
        delay_put(g_delay_w, delay_s16(wr));
        if (++g_delay_w >= RING_DELAY_N) g_delay_w = 0;
        if (++g_delay_cycle_pos >= cycle_len) {
            g_delay_cycle_pos = 0;
            if (g_delay_repeats > 0 && g_delay_repeat_cycle < g_delay_repeats)
                ++g_delay_repeat_cycle;
        }
    }
}

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
    for (int i = 0; i < RING_DELAY_N; ++i) delay_put(i, 0);
    delay_ap_l.Clear(); delay_ap_r.Clear();
    g_wow=0.0f; g_wow_step=0.00001179f; g_feedback_diff=g_delay_hp=0.0f;
    g_delay_w = g_delay_phase = 0;
    g_delay_acc = g_delay_in_lp = g_delay_fb_lp = g_delay_qerr = 0.0f;
    g_delay_repeat_cycle = g_delay_cycle_pos = 0;
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
    g_delay_repeat_cycle = 0; g_delay_cycle_pos = 0;
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
extern "C" void rings_port_set_ambient(int diffusion, int motion, int reverb_send) {
    g_diff=norm100(diffusion); g_motion=norm100(motion); g_delay_rev=norm100(reverb_send);
}
extern "C" void rings_port_set_delay(int time_0_100, int feedback_0_85, int mix_0_100, int repeats_0_8) {
    g_delay_time = norm100(time_0_100);
    if (feedback_0_85 < 0) feedback_0_85 = 0; if (feedback_0_85 > 85) feedback_0_85 = 85;
    g_delay_fb = (float)feedback_0_85 * 0.01f;
    g_delay_mix = norm100(mix_0_100);
    /* Cache the nonlinear Mix curve at control rate, not per audio sample.
     * Mix 50 -> ~0.95x wet return; Mix 100 -> 1.35x wet return. */
    g_delay_wet_gain = g_delay_mix > 0.0f ? 1.35f * sqrtf(g_delay_mix) : 0.0f;
    if (repeats_0_8 < 0) repeats_0_8 = 0; if (repeats_0_8 > 8) repeats_0_8 = 8;
    g_delay_repeats = repeats_0_8;
    g_delay_repeat_cycle = 0; g_delay_cycle_pos = 0;
}
extern "C" void rings_port_set_modulation(float s7,int d7,int a7,float s8,int d8,int a8,float s9,int d9,int a9) {
    float model_sum = 0.0f;
    g_patch = g_base_patch; g_pitch_mod = 0.0f; g_level = g_base_level; g_exciter = g_base_exciter;
    mod_one(s7,d7,a7,&model_sum); mod_one(s8,d8,a8,&model_sum); mod_one(s9,d9,a9,&model_sum);
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
    float in[rings::kMaxBlockSize];
    float out[rings::kMaxBlockSize];
    float aux[rings::kMaxBlockSize];
    while (n) {
        uint32_t k = n > rings::kMaxBlockSize ? rings::kMaxBlockSize : n;
        for (uint32_t i=0;i<k;i++) in[i]=0.0f;
        g_state.note = (float)g_pending_note + g_pitch_mod + g_tune;
        g_state.fm = 0.0f;
        rings::Patch render_patch = g_patch;
        render_patch.brightness = clampf(render_patch.brightness + (g_exciter - 0.5f) * 0.34f, 0.0f, 1.0f);
        g_state.strum = g_pending_strum != 0;
        if (g_state.strum) g_pending_strum = 0;
        g_part.Process(g_state,render_patch,in,out,aux,k);
        g_state.strum = false;
        if (!g_muted) {
            float vel=(float)g_pending_velocity*(1.0f/127.0f);
            float gain=g_level*(0.35f+0.65f*vel)*(0.72f+0.56f*g_exciter);
            for(uint32_t i=0;i<k;i++){
                float fade = 1.0f;
                if (g_model_fade_left > 0 && g_model_fade_total > 0) {
                    fade = 1.0f - (float)g_model_fade_left / (float)g_model_fade_total;
                    if (--g_model_fade_left < 0) g_model_fade_left = 0;
                }
                float lo=out[i]*gain*fade, ro=aux[i]*gain*fade;
                float dlo, dro;
                ring_delay_process(lo, ro, &dlo, &dro);
                left[i]+=dlo; right[i]+=dro;
                if(reverb_send) reverb_send[i]+=0.5f*(lo+ro)*send_amount + 0.5f*((dlo-lo)+(dro-ro))*g_delay_rev;
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
