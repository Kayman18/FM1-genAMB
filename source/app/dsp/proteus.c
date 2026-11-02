/* SPDX-License-Identifier: MIT
 * genAMB / FM-1 adaptation of Seaside Modular Proteus generative sequencer.
 * Original Proteus (c) Seaside Modular contributors; MIT licensed:
 * https://github.com/abluenautilus/SeasideModularVCV/blob/master/src/Proteus.cpp
 *
 * Preserves the original 32-step weighted Repeat/Up/Down/New melody engine,
 * density-rest shuffle, Poisson patience CDF, octave walk, note mutation,
 * and normal/no-mutation/locked modes. VCV Rack CV/UI, expander and JSON
 * are adapted to FM-1 controls, a fixed saved bank, and the shared clock.
 * The user's harmonic requirement replaces the original scale selection:
 * playback maps chord-degree ranks to the live Original-progression chord.
 */
#define PROT_MAGIC 0x50525431u /* PRT1 */
static en_proteus_bank_t prot_bank;
static volatile uint32_t prot_version;
#include <string.h>
volatile uint8_t en_prot_pos[2];
volatile uint8_t en_prot_trace[2][32];
#ifdef PLAT_HOST
volatile uint32_t prot_note_count[2], prot_chord_failure[2];
#endif

static int prot_base(int layer) { return layer ? P_FPROTLEN : P_RPROTLEN; }
static uint32_t prot_random(en_proteus_state_t *s)
{
    uint32_t x = s->rng;
    if (!x) x = 0xB12E9ACDu;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    s->rng = x;
    return x;
}
static int prot_rand(en_proteus_state_t *s, int max)
{
    return max ? (int)((prot_random(s) >> 8) % (uint32_t)max) : 0;
}
static void prot_shuffle(en_proteus_state_t *s, int n)
{
    for (int j = 0; j < 32; ++j) s->restorder[j] = (uint8_t)j;
    for (int j = n - 1; j > 0; --j) {
        int k = prot_rand(s, j + 1);
        uint8_t t = s->restorder[j];
        s->restorder[j] = s->restorder[k]; s->restorder[k] = t;
    }
}
/* Preserve the original "at least one note" Density floor. A shuffled
 * permutation makes adding/removing rests continuous when Density changes. */
static int prot_muted(const en_proteus_state_t *s, int step, int density)
{
    int n = (int)s->length;
    int rests = (n * (100 - density) + 99) / 100;
    if (rests >= n) rests = n - 1;
    for (int i = 0; i < rests; ++i)
        if (s->restorder[i] == step) return 1;
    return 0;
}
static void prot_publish(int layer)
{
    en_proteus_state_t *s = &prot_bank.seq[layer];
    int density = par[prot_base(layer) + 1];
    en_prot_pos[layer] = s->pos;
    for (int i = 0; i < 32; ++i)
        en_prot_trace[layer][i] = i >= s->length ? 0 :
            (uint8_t)((s->note[i] * 7u) | (prot_muted(s,i,density) ? 0 : 128u));
}
/* Notes use relative chord tones. 0..15 fits up to four chord tones across
 * four octaves; no new tonal material is introduced on chord changes. */
static void prot_generate(int layer)
{
    en_proteus_state_t *s = &prot_bank.seq[layer];
    int b = prot_base(layer), n = par[b];
    if (n < 1) n = 1;
    if (n > 32) n = 32;
    s->length=(uint8_t)n;
    s->pos=s->repeats=0;
    s->octave_shift=0;
    prot_shuffle(s,n);
    int prev = 4;
    for (int i=0;i<32;++i) {
        /* Proteus source weights: repeat 5, up 5, down 5, new 10. */
        int choice = i ? prot_rand(s,25) : 16;
        if (choice < 5) { /* repeat */ }
        else if (choice < 10) { if (prev < 15) prev++; }
        else if (choice < 15) { if (prev > 0) prev--; }
        else prev = prot_rand(s,12);
        s->note[i] = (uint8_t)prev;
    }
    prot_publish(layer);
}
static void prot_init(void)
{
    prot_version=0; prot_bank.magic=PROT_MAGIC;
    for (int layer=0; layer<2;++layer) {
        en_proteus_state_t *s=&prot_bank.seq[layer];
        s->rng=layer ? 0x57BD3629u : 0xA18EBD71u;
        prot_generate(layer);
    }
}
void en_proteus_snapshot(en_proteus_bank_t *out)
{
    uint32_t a,b;
    do {
        a=prot_version; BARRIER();
        memcpy(out, &prot_bank, sizeof *out);
        BARRIER(); b=prot_version;
    } while ((a&1u) || a!=b);
}
void en_proteus_restore_boot(const en_proteus_bank_t *in)
{
    if (in->magic!=PROT_MAGIC) return;
    for (int layer=0;layer<2;++layer) {
        if (!in->seq[layer].rng || in->seq[layer].length<1 ||
            in->seq[layer].length>32 || in->seq[layer].pos>=in->seq[layer].length)
            return;
        for (int i=0;i<32;++i)
            if (in->seq[layer].note[i]>15 || in->seq[layer].restorder[i]>31)
                return;
    }
    memcpy(&prot_bank,in,sizeof prot_bank);
    for (int layer=0;layer<2;++layer) prot_publish(layer);
}
static void prot_new(int layer)
{
    if ((unsigned)layer>=2u) return;
    prot_version++; BARRIER();
    prot_generate(layer);
    BARRIER(); prot_version++;
}
static void prot_reset_position(int layer)
{
    prot_bank.seq[layer].pos=0;
    en_prot_pos[layer]=0;
}
/* The reference does cdfpoi(&p, repeats, lambda) each loop: exactly the
 * Poisson CDF P(X<=repeats) with X~Poisson(lambda). CDF expressed as a
 * bounded recurrence to avoid pulling heavyweight host math onto CPU0. */
static float prot_poisson_cdf(int rep, int lambda)
{
    float t=1.0f, p;
    for (int i=0;i<lambda;++i) t*=0.36787944117144233f; /* exp(-lambda) */
    p=t;
    if (rep>250) rep=250;
    for (int j=1;j<=rep;++j) {
        t*=((float)lambda/(float)j);
        p+=t;
        if (p>=0.999999f) return 1.0f;
    }
    return p>1.0f?1.0f:p;
}
/* Gate controls the FM note-off period; Rings is a resonator, so its
 * percussive excitation responds to Gate as velocity instead of a sustain. */
static uint32_t prot_gate_samples(void)
{
    float sixteenth_samples = en_ext ? fm_quarter_samples * 0.25f : step_len;
    float samples=sixteenth_samples*(float)evolve_rate_divisor(par[P_FPROTRATE])*
        (float)par[P_FPROTGATE]*0.01f;
    if (samples<1.0f) samples=1.0f;
    if (samples>EN_SR*8.0f) samples=EN_SR*8.0f;
    return (uint32_t)(samples+0.5f);
}
static void prot_tick(int layer)
{
    en_proteus_state_t *s=&prot_bank.seq[layer];
    int b=prot_base(layer),len=par[b],density=par[b+1];
    if (len<1) len=1;
    if (len>32)len=32;
    if (s->length!=len) {
        prot_version++; BARRIER();
        s->length=(uint8_t)len; s->pos=0;
        prot_shuffle(s,len);
        BARRIER();prot_version++;
    }
    int i=s->pos;
    en_prot_pos[layer]=(uint8_t)i;
    if (!prot_muted(s,i,density) && chord_nt>0 && nsteps>0) {
        int base_midi = 60 + par[P_ROOT] + 12 * par[layer?P_FMOCT:P_BOCT];
        int rank = s->note[i], degree = rank % chord_nt;
        /* Chord IDs come from Original's global progression, not Evolve's
         * optional Scale follow. Live remapping never rewrites the melody. */
        int pc=chord_pcs[degree];
        int rel=(pc - par[P_ROOT] + 12) % 12;
        int pitch=base_midi+rel+12*(rank/chord_nt+s->octave_shift);
        pitch=seq_note(pitch,par[layer?P_FMRANGE:P_RINGRANGE],
            par[layer?P_FMCHANCE:P_RINGCHANCE],frand(),frand());
        while(pitch>127)pitch-=12;
        while(pitch<0)pitch+=12;
#ifdef PLAT_HOST
        prot_note_count[layer]++;
        int ok=0;
        for(int j=0;j<chord_nt;++j)if(pitch%12==chord_pcs[j])ok=1;
        if(!ok)prot_chord_failure[layer]++;
#endif
        if(layer) {
            if(par[P_FMLEVEL]>0) fm_seq_schedule(pitch,92,0);
        } else {
            float strike=0.35f+0.65f*(float)par[P_RPROTGATE]*0.01f;
            ring_schedule(pitch,(int)(strike*110.0f),0);
            midi(0x91,(uint32_t)pitch,(uint32_t)(strike*110.0f));
            midi(0x81,(uint32_t)pitch,0);
        }
    }
    prot_version++; BARRIER();
    s->pos++;
    if (s->pos>=len) {
        s->pos=0;
        if (par[b+7]<2) { /* normal or no-mutation, both accumulate patience */
            if(s->repeats<250) s->repeats++;
            float p=prot_poisson_cdf(s->repeats,par[b+2]);
            if((prot_random(s)>>8) < (uint32_t)(p*16777216.0f)) {
                prot_generate(layer);
            } else if (par[b+7]==0) { /* normal: mutate octave and one note */
                if (prot_rand(s,100)<par[b+5]) {
                    int max=par[b+8];
                    int delta=prot_rand(s,2)?1:-1;
                    if(s->octave_shift+delta>max || s->octave_shift+delta<-max) delta=-delta;
                    if(s->octave_shift+delta<=max && s->octave_shift+delta>=-max)
                        s->octave_shift=(int8_t)(s->octave_shift+delta);
                }
                if(prot_rand(s,100)<par[b+6]) {
                    int at=prot_rand(s,len);
                    s->note[at]=(uint8_t)prot_rand(s,12);
                }
            }
        }
    }
    BARRIER();prot_version++;
    prot_publish(layer);
}
