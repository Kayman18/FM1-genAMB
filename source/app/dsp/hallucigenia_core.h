/* SPDX-License-Identifier: GPL-3.0-or-later
 * Hallucigenia / Darwinism by Eater of Sheep (Extratone), adapted for genAMB.
 * Source: app/vendor/hallucigenia/Darwinism.cpp (see UPSTREAM.md).
 * The source's randomiseStep, mutation loop and clock advancement are retained
 * with Rack parameters/ports replaced by fixed state and event calls.
 */
#ifndef GENAMB_HALLUCIGENIA_CORE_H
#define GENAMB_HALLUCIGENIA_CORE_H
#include <stdint.h>
typedef struct { float note[16]; uint8_t on[16]; } hall_state_t;
typedef float (*hall_uniform_t)(void);
static void hall_randomise_step(hall_state_t *s,int i,float density,hall_uniform_t uniform)
{
    s->note[i]=uniform();
    s->on[i]=(uint8_t)(uniform()<density);
}
static void hall_randomise(hall_state_t *s,float density,hall_uniform_t uniform)
{
    for(int i=0;i<16;i++)hall_randomise_step(s,i,density,uniform);
}
static void hall_mutate(hall_state_t *s,float mutationrate,float density,hall_uniform_t uniform)
{
    for(int i=0;i<16;i++)if(uniform()<mutationrate)hall_randomise_step(s,i,density,uniform);
}
/* The upstream increments BEFORE reading the note/gate; reset is step zero.
 * Returns end-of-pattern so the FM-1 adapter can wire it to mutation. */
static int hall_clock(uint8_t *step,int length)
{
    if(length<2)length=2;
    if(length>16)length=16;
    if(*step<length-1){(*step)++;return 0;}
    *step=0;return 1;
}
/* Preserve the existing project format: quantize only when storing a pattern.
 * Runtime mutation keeps the upstream's floating-point note values. */
static void hall_store(const hall_state_t *s,volatile uint8_t out[16])
{
    for(int i=0;i<16;i++){
        int n=(int)(s->note[i]*128.0f);if(n<0)n=0;if(n>127)n=127;
        out[i]=(uint8_t)(n|(s->on[i]?128:0));
    }
}
static void hall_load(hall_state_t *s,const volatile uint8_t in[16])
{
    for(int i=0;i<16;i++){s->note[i]=(float)(in[i]&127u)*(1.0f/128.0f);s->on[i]=(in[i]>>7)&1u;}
}
#endif
