/* SPDX-License-Identifier: GPL-3.0-or-later
 * FM-1 Evolve: compact, clock-driven generative sequencing.
 * Upstream Hallucigenia/Darwinism adaptation; see hallucigenia_core.h.
 * This file is included by amb.c; all writes occur on CPU0 in the audio engine.
 */
#include "hallucigenia_core.h"
static hall_state_t evo_core;
#define EVO_MAGIC 0x45564F31u
static volatile en_evo_bank_t evo_bank;
static volatile uint32_t evo_version;
volatile uint8_t en_evo_active, en_evo_pattern, en_evo_pos;
volatile uint8_t en_ring_seq_active_mode;
volatile uint8_t en_evo_trace[16];
static uint8_t evo_next, evo_wrapped;

static uint32_t evo_random(void)
{
    uint32_t x=evo_bank.rng;
    x^=x<<13; x^=x>>17; x^=x<<5;
    evo_bank.rng=x; return x;
}
static float evo_uniform(void) { return (float)(evo_random()>>8)*(1.0f/16777216.0f); }
static uint8_t evo_step_new(int density)
{
    float note=evo_uniform();
    int on=evo_uniform()<(float)density*0.01f;
    return (uint8_t)((int)(note*128.0f)|(on?128:0));
}
static void evo_publish(void)
{
    for(int i=0;i<16;i++) en_evo_trace[i]=evo_bank.step[en_evo_pattern][i];
}
static void evo_init(void)
{
    evo_version=0; evo_bank.magic=EVO_MAGIC; evo_bank.rng=0x9384B71Du;
    en_evo_active=en_evo_pattern=en_evo_pos=evo_next=evo_wrapped=0;
    en_ring_seq_active_mode=0;
    for(int p=0;p<16;p++)for(int i=0;i<16;i++) evo_bank.step[p][i]=evo_step_new(70);
    hall_load(&evo_core,evo_bank.step[en_evo_pattern]);
    evo_publish();
}
void en_evo_snapshot(en_evo_bank_t *out)
{
    uint32_t before,after;
    do {
        before=evo_version; BARRIER();
        out->magic=evo_bank.magic;out->rng=evo_bank.rng;
        for(int p=0;p<16;p++)for(int i=0;i<16;i++)out->step[p][i]=evo_bank.step[p][i];
        BARRIER();after=evo_version;
    } while((before&1u)||before!=after);
}
void en_evo_restore_boot(const en_evo_bank_t *in)
{
    if(in->magic!=EVO_MAGIC || !in->rng)return;
    evo_bank.magic=in->magic;evo_bank.rng=in->rng;
    for(int p=0;p<16;p++)for(int i=0;i<16;i++)evo_bank.step[p][i]=in->step[p][i];
    hall_load(&evo_core,evo_bank.step[en_evo_pattern]);
    evo_publish();
}
static void evo_select(void)
{
    int p=par[P_EPATTERN]-1;
    if(p<0||p>15)p=0;
    if(en_ring_seq_active_mode != par[P_RSEQMODE]) {
        /* Switch at the next bar; cancel pending strikes, preserve audible tails. */
        rings_wait_clear();
        en_ring_seq_active_mode=(uint8_t)par[P_RSEQMODE];
    }
    en_evo_active=(uint8_t)(en_ring_seq_active_mode==1);
    if(en_evo_pattern!=p) {
        en_evo_pattern=(uint8_t)p;evo_next=evo_wrapped=0;en_evo_pos=0;
        hall_load(&evo_core,evo_bank.step[p]);
    }
    evo_publish();
}
static void evo_randomize(void)
{
    int target=par[P_EPATTERN]-1; if(target<0||target>15)target=0;
    evo_version++; BARRIER();
    hall_state_t generated;
    hall_state_t *state=target==en_evo_pattern?&evo_core:&generated;
    hall_randomise(state,(float)par[P_EDENSITY]*0.01f,evo_uniform);
    hall_store(state,evo_bank.step[target]);
    BARRIER();evo_version++;
    evo_publish();
}
static int evo_pitch(float value)
{
    /* Rank quantization: no notes outside the current harmony, including roots
     * near the octave boundary. Octave drift is applied after this mapping. */
    int base=60+par[P_ROOT]+12*par[P_BOCT], count=0, chosen=base;
    int span=12*par[P_ERANGE];
    for(int n=base;n<base+span;n++) {
        int ok=0, pc=n%12;
        if(par[P_EFOLLOW]) {for(int j=0;j<chord_nt;j++)if(pc==chord_pcs[j])ok=1;}
        else {for(int j=0;j<nsteps;j++)if(pc==(par[P_ROOT]+steps[j])%12)ok=1;}
        count+=ok;
    }
    if(!count)return base;
    int rank=(int)(value*(float)count);
    for(int n=base;n<base+span;n++) {
        int ok=0,pc=n%12;
        if(par[P_EFOLLOW]) {for(int j=0;j<chord_nt;j++)if(pc==chord_pcs[j])ok=1;}
        else {for(int j=0;j<nsteps;j++)if(pc==(par[P_ROOT]+steps[j])%12)ok=1;}
        if(ok && rank--==0){chosen=n;break;}
    }
    return chosen;
}
static void evo_tick(void)
{
    evo_wrapped=(uint8_t)hall_clock(&evo_next,par[P_ELENGTH]);
    en_evo_pos=evo_next;
    /* Upstream S&H-on-trigger captures pitch before the mutation trigger. */
    int triggered=evo_core.on[evo_next];
    float note=evo_core.note[evo_next];
    if(evo_wrapped) {
        evo_version++;BARRIER();
        hall_mutate(&evo_core,(float)par[P_EMUTATE]*0.01f,(float)par[P_EDENSITY]*0.01f,evo_uniform);
        hall_store(&evo_core,evo_bank.step[en_evo_pattern]);
        BARRIER();evo_version++;evo_publish();
    }
    if(triggered) {
        int n=seq_note(evo_pitch(note),par[P_RINGRANGE],par[P_RINGCHANCE],frand(),frand());
        /* Fold into MIDI bounds by octaves, preserving the harmonic pitch class. */
        while(n>127)n-=12;while(n<0)n+=12;
        ring_sequence_strike(n,0.8f,0);
    }
}
