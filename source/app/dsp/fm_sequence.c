/* SPDX-License-Identifier: GPL-3.0-or-later
 * Original sequencer and upstream Hallucigenia adaptation for the Zen FM layer.
 * Hallucigenia portions derive from Eater of Sheep / Extratone; see vendor/hallucigenia.
 * Pre-existing Original sequencer portions retain their MPL-2.0 notices.
 * Included by amb.c after evolve.c. All sequence and MIDI writes stay on CPU0;
 * only the FM voice renderer runs in the joined audio worker.
 */
#include "evolve_rate.h"
#include "hallucigenia_core.h"
static hall_state_t fm_evo_core;
#include "octave_pitch.h"
#include "../fm_port/fm_port.h"

#define FM_EVO_MAGIC 0x464D4531u
#define FM_NWAIT 32
#define FM_MIDI_VOICES FM_PORT_VOICES

static uint32_t fm_seq_rng;
static int8_t fm_loop_note[EN_NLOOP][EN_LOOP_MAX];
static uint8_t fm_loop_vel[EN_NLOOP][EN_LOOP_MAX];
static uint8_t fm_loop_pos[EN_NLOOP], fm_loop_rest[EN_NLOOP];
volatile uint8_t en_fm_loop_pos[EN_NLOOP];
volatile int8_t en_fm_loop_note[EN_NLOOP][EN_LOOP_MAX];
volatile float en_fm_loop_glow[EN_NLOOP];

static volatile en_evo_bank_t fm_evo_bank;
static volatile uint32_t fm_evo_version;
volatile uint8_t en_fm_evo_active, en_fm_evo_pattern, en_fm_evo_pos;
volatile uint8_t en_fm_seq_active_mode;
volatile uint8_t en_fm_evo_trace[16];
static uint8_t fm_evo_next, fm_evo_wrapped;

typedef struct { int32_t wait; uint8_t note, velocity, active; } fm_seq_wait_t;
static fm_seq_wait_t fm_seq_wait[FM_NWAIT];
typedef struct { int32_t left; uint8_t note, active; } fm_midi_gate_t;
static fm_midi_gate_t fm_midi_gate[FM_MIDI_VOICES];
static uint32_t fm_tick_age;
static float fm_quarter_samples;
static uint8_t fm_tick_seen;

static uint32_t fm_seq_random(void)
{
    uint32_t x = fm_seq_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    fm_seq_rng = x; return x;
}
static float fm_seq_frand(void)
{
    return (float)(fm_seq_random() >> 8) * (1.0f / 16777216.0f);
}
static int fm_seq_irand(int n)
{
    return n > 0 ? (int)((fm_seq_random() >> 8) % (uint32_t)n) : 0;
}
static uint32_t fm_evo_random(void)
{
    uint32_t x = fm_evo_bank.rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    fm_evo_bank.rng = x; return x;
}
static float fm_evo_uniform(void) { return (float)(fm_evo_random()>>8)*(1.0f/16777216.0f); }
static uint8_t fm_evo_step_new(int density)
{
    float note=fm_evo_uniform();
    int on=fm_evo_uniform()<(float)density*0.01f;
    return (uint8_t)((int)(note*128.0f)|(on?128:0));
}
static void fm_evo_publish(void)
{
    for (int i = 0; i < 16; ++i)
        en_fm_evo_trace[i] = fm_evo_bank.step[en_fm_evo_pattern][i];
}
void en_fmevo_snapshot(en_evo_bank_t *out)
{
    uint32_t before, after;
    do {
        before = fm_evo_version; BARRIER();
        out->magic = fm_evo_bank.magic; out->rng = fm_evo_bank.rng;
        for (int p = 0; p < 16; ++p)
            for (int i = 0; i < 16; ++i) out->step[p][i] = fm_evo_bank.step[p][i];
        BARRIER(); after = fm_evo_version;
    } while ((before & 1u) || before != after);
}
void en_fmevo_restore_boot(const en_evo_bank_t *in)
{
    if (in->magic != FM_EVO_MAGIC || !in->rng) return;
    fm_evo_bank.magic = in->magic; fm_evo_bank.rng = in->rng;
    for (int p = 0; p < 16; ++p)
        for (int i = 0; i < 16; ++i) fm_evo_bank.step[p][i] = in->step[p][i];
    hall_load(&fm_evo_core,fm_evo_bank.step[en_fm_evo_pattern]);
    fm_evo_publish();
}

static int32_t fm_rate_wait = -1;
static void fm_rate_fire_due(void);
static void fm_seq_clear(void)
{
    fm_rate_wait=-1;
    for (int i = 0; i < FM_NWAIT; ++i) fm_seq_wait[i].active = 0;
}
static void fm_seq_midi_off_all(void)
{
    for (int i = 0; i < FM_MIDI_VOICES; ++i) {
        if (fm_midi_gate[i].active) {
            /* An already-emitted note must end even if MIDI out was disabled
             * after its note-on. This does not enable new MIDI output. */
            en_midi_out(0x82u, fm_midi_gate[i].note, 0);
            fm_midi_gate[i].active = 0;
        }
    }
}
static void fm_seq_stop(void)
{
    fm_seq_clear(); fm_seq_midi_off_all();
    fm_tick_age = 0; fm_tick_seen = 0;
}
static void fm_seq_midi_on(int note, int velocity, uint32_t gate)
{
    int slot = -1, poly = par[P_FMPOLY];
    if (!par[P_MIDI]) return;
    if (poly < 1) poly = 1;
    if (poly > FM_MIDI_VOICES) poly = FM_MIDI_VOICES;
    for (int i = 0; i < FM_MIDI_VOICES; ++i) {
        if (fm_midi_gate[i].active && (i >= poly || fm_midi_gate[i].note == note)) {
            en_midi_out(0x82u, fm_midi_gate[i].note, 0);
            fm_midi_gate[i].active = 0;
        }
        if (i < poly && !fm_midi_gate[i].active && slot < 0) slot = i;
    }
    if (slot < 0) {
        slot = 0;
        for (int i = 1; i < poly; ++i)
            if (fm_midi_gate[i].left < fm_midi_gate[slot].left) slot = i;
        en_midi_out(0x82u, fm_midi_gate[slot].note, 0);
    }
    fm_midi_gate[slot].left = (int32_t)gate;
    fm_midi_gate[slot].note = (uint8_t)note;
    fm_midi_gate[slot].active = 1;
    en_midi_out(0x92u, (uint32_t)note, (uint32_t)velocity);
}
static uint32_t fm_seq_gate_samples(void)
{
    float quarter = en_ext ? fm_quarter_samples : step_len * 4.0f;
    if (quarter < 64.0f) quarter = 64.0f;
    if (quarter > 8.0f * EN_SR) quarter = 8.0f * EN_SR;
    float gate = quarter * (float)par[P_FMGATE] * 0.01f;
    if (gate < 1.0f) gate = 1.0f;
    return (uint32_t)(gate + 0.5f);
}
static void fm_seq_schedule(int note, int velocity, int32_t wait)
{
    int slot = -1;
    /* Fold by octaves rather than clamp: preserve the harmony at MIDI bounds. */
    while (note < 0) note += 12;
    while (note > 127) note -= 12;
    if (velocity < 1) velocity = 1;
    if (velocity > 127) velocity = 127;
    if (wait < 0) wait = 0;
    for (int i = 0; i < FM_NWAIT; ++i)
        if (!fm_seq_wait[i].active) { slot = i; break; }
    if (slot < 0) {
        /* Keep the nearer event under extreme external clocks. FM supports
         * several simultaneous note-ons, so no artificial 1-sample staggering. */
        int32_t latest = -1;
        for (int i = 0; i < FM_NWAIT; ++i)
            if (fm_seq_wait[i].wait > latest) { latest = fm_seq_wait[i].wait; slot = i; }
        if (latest <= wait) return;
    }
    fm_seq_wait[slot].wait = wait;
    fm_seq_wait[slot].note = (uint8_t)note;
    fm_seq_wait[slot].velocity = (uint8_t)velocity;
    fm_seq_wait[slot].active = 1;
}
static void fm_seq_fire_due(void)
{
    fm_rate_fire_due();
    for (int i = 0; i < FM_MIDI_VOICES; ++i)
        if (fm_midi_gate[i].active && fm_midi_gate[i].left <= 0) {
            en_midi_out(0x82u, fm_midi_gate[i].note, 0);
            fm_midi_gate[i].active = 0;
        }
    for (int i = 0; i < FM_NWAIT; ++i)
        if (fm_seq_wait[i].active && fm_seq_wait[i].wait <= 0) {
            if (par[P_FMLEVEL] > 0 && par[P_MODE] != MODE_PIANO) {
                uint32_t gate = en_fm_seq_active_mode == 3 ? ud_gate_samples() : en_fm_seq_active_mode == 2 ? prot_gate_samples() : fm_seq_gate_samples();
                int note = fm_seq_wait[i].note, velocity = fm_seq_wait[i].velocity;
                fm_port_trigger(note, velocity, gate);
                fm_seq_midi_on(note, velocity, gate);
            }
            fm_seq_wait[i].active = 0;
        }
}
static int32_t fm_seq_wait_next(void)
{
    int32_t next = fm_rate_wait>0?fm_rate_wait:0;
    for (int i = 0; i < FM_NWAIT; ++i)
        if (fm_seq_wait[i].active && fm_seq_wait[i].wait > 0 &&
            (!next || fm_seq_wait[i].wait < next)) next = fm_seq_wait[i].wait;
    for (int i = 0; i < FM_MIDI_VOICES; ++i)
        if (fm_midi_gate[i].active && fm_midi_gate[i].left > 0 &&
            (!next || fm_midi_gate[i].left < next)) next = fm_midi_gate[i].left;
    return next;
}
static void fm_seq_step(int32_t n)
{
    if(fm_rate_wait>=0)fm_rate_wait-=n;
    for (int i = 0; i < FM_NWAIT; ++i)
        if (fm_seq_wait[i].active) fm_seq_wait[i].wait -= n;
    for (int i = 0; i < FM_MIDI_VOICES; ++i)
        if (fm_midi_gate[i].active) fm_midi_gate[i].left -= n;
    if (fm_tick_age < (uint32_t)(EN_SR * 60.0f)) fm_tick_age += (uint32_t)n;
    for (int l = 0; l < EN_NLOOP; ++l) en_fm_loop_glow[l] *= 0.985f;
}

static int fm_original_offset(void)
{
    if (fm_seq_frand() < 0.72f) {
        int o = 2 * fm_seq_irand(chord_nt);
        return o + (fm_seq_frand() < 0.3f ? nsteps : 0);
    }
    return 1 + 2 * fm_seq_irand(3);
}
static int fm_original_count(int l)
{
    int c = 0;
    for (int i = 0; i < EN_LOOP_LEN[l]; ++i) c += fm_loop_note[l][i] >= 0;
    return c;
}
static int fm_original_target(void)
{
    float d = (float)par[P_FMDENSITY] * 0.05f * (1.0f + 0.8f * arcm);
    return (int)(fm_clampf(d, 0.0f, 5.0f) + fm_seq_frand());
}
static void fm_original_add(int l)
{
    int len = EN_LOOP_LEN[l];
    for (int i = 0; i < 8; ++i) {
        int s = fm_seq_irand(len);
        if (fm_loop_note[l][s] < 0) {
            fm_loop_note[l][s] = (int8_t)fm_original_offset();
            fm_loop_vel[l][s] = (uint8_t)(50 + fm_seq_irand(50));
            return;
        }
    }
}
static int fm_original_pick(int l)
{
    int len = EN_LOOP_LEN[l], s = fm_seq_irand(len);
    for (int i = 0; i < len; ++i, s = (s + 1) % len)
        if (fm_loop_note[l][s] >= 0) return s;
    return -1;
}
static void fm_original_publish(int l)
{
    for (int i = 0; i < EN_LOOP_MAX; ++i)
        en_fm_loop_note[l][i] = i < EN_LOOP_LEN[l] ? fm_loop_note[l][i] : -1;
}
static void fm_seq_regrow(void)
{
    for (int l = 0; l < EN_NLOOP; ++l) {
        int n = fm_original_target();
        for (int i = 0; i < EN_LOOP_MAX; ++i) fm_loop_note[l][i] = -1;
        for (int i = 0; i < n; ++i) fm_original_add(l);
        fm_loop_rest[l] = 0; fm_original_publish(l);
    }
}
static void fm_original_pass(int l)
{
    int c = fm_original_count(l), t = fm_original_target(), s;
    for (; c < t; ++c) fm_original_add(l);
    for (; c > t && (s = fm_original_pick(l)) >= 0; --c) fm_loop_note[l][s] = -1;
    fm_loop_rest[l] = fm_seq_frand() < evolve * 0.15f + 0.15f * fm_maxf(-arcm, 0.0f);
    if (fm_seq_frand() < evolve * 0.7f && (s = fm_original_pick(l)) >= 0) {
        if (fm_seq_frand() < 0.5f) fm_loop_note[l][s] = (int8_t)fm_original_offset();
        else {
            int d = fm_seq_irand(EN_LOOP_LEN[l]);
            if (fm_loop_note[l][d] < 0) {
                fm_loop_note[l][d] = fm_loop_note[l][s];
                fm_loop_vel[l][d] = fm_loop_vel[l][s]; fm_loop_note[l][s] = -1;
            }
        }
    }
    fm_original_publish(l);
}
static int fm_original_pitch(int l, int off)
{
    if (!nsteps) return 60 + par[P_ROOT];
    int d = deg + off, oct = d / nsteps, s = steps[d % nsteps], root = par[P_ROOT];
    int base = LOOP_BASE[l] - ((LOOP_BASE[l] - root) % 12 + 12) % 12;
    int n = base + s + 12 * oct;
    return en_original_octave_pitch(n, par[P_FMOCT]);
}
static void fm_sequence_strike(int note, float velocity, int l)
{
    int v = (int)(velocity * 110.0f);
    int32_t wait = fm_seq_irand(2600);
    fm_seq_schedule(note, v, wait); en_fm_loop_glow[l] = 1.0f;
    if (par[P_FMBURST] > 0 && chord_nt > 0 &&
        fm_seq_frand() < (float)par[P_FMBURST] * 0.01f) {
        int extra = 1 + fm_seq_irand(3), start = fm_seq_irand(chord_nt), last = note;
        for (int j = 0; j < extra; ++j) {
            int nn = (note / 12) * 12 + chord_pcs[(start + j) % chord_nt];
            while (nn > 127) nn -= 12;
            if (nn == last) break;
            wait += 3 * (520 + fm_seq_irand(1024));
            int vv = v - 12 - j * 8 + fm_seq_irand(7);
            if (vv < 25) vv = 25;
            fm_seq_schedule(nn, vv, wait); last = nn;
        }
    }
}
static int fm_evo_pitch(float value)
{
    int base = 60 + par[P_ROOT] + 12 * par[P_FMOCT], count = 0, chosen = base;
    int span = 12 * par[P_FMERANGE];
    for (int n = base; n < base + span; ++n) {
        int ok = 0, pc = n % 12;
        if (par[P_FMEFOLLOW]) {
            for (int j = 0; j < chord_nt; ++j) if (pc == chord_pcs[j]) ok = 1;
        } else {
            for (int j = 0; j < nsteps; ++j) if (pc == (par[P_ROOT] + steps[j]) % 12) ok = 1;
        }
        count += ok;
    }
    if (!count) return base;
    int rank = (int)(value * (float)count);
    for (int n = base; n < base + span; ++n) {
        int ok = 0, pc = n % 12;
        if (par[P_FMEFOLLOW]) {
            for (int j = 0; j < chord_nt; ++j) if (pc == chord_pcs[j]) ok = 1;
        } else {
            for (int j = 0; j < nsteps; ++j) if (pc == (par[P_ROOT] + steps[j]) % 12) ok = 1;
        }
        if (ok && rank-- == 0) { chosen = n; break; }
    }
    return chosen;
}
static void fm_evo_randomize(void)
{
    int target = par[P_FMEPATTERN] - 1;
    if (target < 0 || target > 15) target = 0;
    fm_evo_version++; BARRIER();
    hall_state_t generated;
    hall_state_t *state=target==en_fm_evo_pattern?&fm_evo_core:&generated;
    hall_randomise(state,(float)par[P_FMEDENSITY]*0.01f,fm_evo_uniform);
    hall_store(state,fm_evo_bank.step[target]);
    BARRIER(); fm_evo_version++; fm_evo_publish();
}
static void fm_seq_select(void)
{
    int p = par[P_FMEPATTERN] - 1;
    if (p < 0 || p > 15) p = 0;
    if (en_fm_seq_active_mode != par[P_FMSEQMODE]) {
        fm_seq_clear(); en_fm_seq_active_mode = (uint8_t)par[P_FMSEQMODE];
    }
    en_fm_evo_active = (uint8_t)(en_fm_seq_active_mode == 1);
    if (en_fm_evo_pattern != p) {
        en_fm_evo_pattern = (uint8_t)p;
        fm_evo_next = fm_evo_wrapped = en_fm_evo_pos = 0;
        hall_load(&fm_evo_core,fm_evo_bank.step[p]);
    }
    fm_evo_publish();
}
static void fm_evo_tick(void)
{
    fm_evo_wrapped=(uint8_t)hall_clock(&fm_evo_next,par[P_FMELENGTH]);
    en_fm_evo_pos=fm_evo_next;
    int triggered=fm_evo_core.on[fm_evo_next];
    float note=fm_evo_core.note[fm_evo_next];
    if(fm_evo_wrapped) {
        fm_evo_version++;BARRIER();
        hall_mutate(&fm_evo_core,(float)par[P_FMEMUTATE]*0.01f,(float)par[P_FMEDENSITY]*0.01f,fm_evo_uniform);
        hall_store(&fm_evo_core,fm_evo_bank.step[en_fm_evo_pattern]);
        BARRIER();fm_evo_version++;fm_evo_publish();
    }
    if(triggered && par[P_FMLEVEL]>0) {
        int n = seq_note(fm_evo_pitch(note), par[P_FMRANGE], par[P_FMCHANCE],
                         fm_seq_frand(), fm_seq_frand());
        while (n > 127) n -= 12;
        while (n < 0) n += 12;
        fm_sequence_strike(n, 0.8f, 0);
    }
}
static void fm_seq_restart(void)
{
    fm_seq_stop();
    fm_evo_next = fm_evo_wrapped = en_fm_evo_pos = 0;
    for (int l = 0; l < EN_NLOOP; ++l) fm_loop_pos[l] = en_fm_loop_pos[l] = 0;
    fm_seq_select();
}
static void fm_seq_init(void)
{
    fm_seq_rng = 0xD3147A95u;
    fm_evo_version = 0; fm_evo_bank.magic = FM_EVO_MAGIC; fm_evo_bank.rng = 0x7BC42DE1u;
    en_fm_evo_active = en_fm_evo_pattern = en_fm_evo_pos = fm_evo_next = fm_evo_wrapped = 0;
    en_fm_seq_active_mode = 0;
    for (int p = 0; p < 16; ++p)
        for (int i = 0; i < 16; ++i) fm_evo_bank.step[p][i] = fm_evo_step_new(70);
    for (int l = 0; l < EN_NLOOP; ++l) {
        fm_loop_pos[l] = en_fm_loop_pos[l] = fm_loop_rest[l] = 0;
        en_fm_loop_glow[l] = 0.0f;
    }
    for (int i = 0; i < FM_MIDI_VOICES; ++i) fm_midi_gate[i].active = 0;
    fm_tick_age = 0; fm_tick_seen = 0; fm_quarter_samples = step_len * 4.0f;
    hall_load(&fm_evo_core,fm_evo_bank.step[en_fm_evo_pattern]);
    fm_seq_clear(); fm_seq_regrow(); fm_seq_select();
}
static void fm_rate_fire_due(void) {
    if(fm_rate_wait!=0)return;
    fm_rate_wait=-1;
    if(par[P_MODE]==MODE_PIANO)return;
    if(en_fm_seq_active_mode==3)ud_tick(1);
    else if(en_fm_seq_active_mode==2)prot_tick(1);
    else if(en_fm_evo_active)fm_evo_tick();
}
static void fm_seq_tick(uint32_t grid)
{
    /* Sixteenths from internal or external clock. Read the elapsed audio samples
     * to give externally clocked notes a real tempo-relative release gate too. */
    if (en_ext && fm_tick_seen && fm_tick_age >= (uint32_t)(EN_SR * 0.001f) &&
        fm_tick_age <= (uint32_t)(EN_SR * 2.0f))
        fm_quarter_samples = (float)fm_tick_age * 4.0f;
    if (!en_ext) fm_quarter_samples = step_len * 4.0f;
    fm_tick_age = 0; fm_tick_seen = 1;
    if ((grid & 15u) == 0u) fm_seq_select();
    if (par[P_MODE] == MODE_PIANO) return;
    fm_rate_wait=-1;
    if(en_fm_seq_active_mode!=0) {
        int p=en_fm_seq_active_mode==3?P_FUDRATE:en_fm_seq_active_mode==2?P_FPROTRATE:P_FMERATE;
        fm_rate_wait=seq_rate_offset(par[p],grid,fm_quarter_samples*0.25f);
        fm_rate_fire_due();
    } else if ((grid & 3u) == 0u) {
        for (int l = 0; l < EN_NLOOP; ++l) {
            int s = fm_loop_pos[l];
            if (par[P_FMLEVEL] > 0 && fm_loop_note[l][s] >= 0 && !fm_loop_rest[l]) {
                int n = seq_note(fm_original_pitch(l, fm_loop_note[l][s]),
                                 par[P_FMRANGE], par[P_FMCHANCE], fm_seq_frand(), fm_seq_frand());
                fm_sequence_strike(n, (float)fm_loop_vel[l][s] * 0.01f, l);
            }
            en_fm_loop_pos[l] = (uint8_t)s;
            if (++fm_loop_pos[l] >= EN_LOOP_LEN[l]) {
                fm_loop_pos[l] = 0; fm_original_pass(l);
            }
        }
    }
}
