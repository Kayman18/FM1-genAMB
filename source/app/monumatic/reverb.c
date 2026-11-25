#include "reverb_math.h"
#include "plat.h"
#include "reverb.h"
#include <math.h>
#include <string.h>
#ifdef MM_FM1_FLOAT_ONLY
#include "fastmath.h"
#ifndef cosf
#define cosf fm_cosf
#endif
#ifndef sqrtf
#define sqrtf fm_sqrtf
#endif
#endif
_Static_assert(sizeof(MM_ReverbLine)==0x28,"source line size");
_Static_assert(sizeof(MM_Reverb)==0x164,"source header size");
static const float parameters[8][4]={
#include "reverb_parameters.inc"
};
#ifdef MM_FM1_FLOAT_ONLY
/* Exact source results for the fixed ROM constants; avoid FP64 setup math. */
static const float maximum_delays[8]={
#include "reverb_maximum_delays.inc"
};
/* Match trunc((double)x + .5) without rounding x+.5 in float first.
 * Callers provide finite values in the source's int32 representable range. */
static inline __attribute__((always_inline)) int round_half(float x){
 int n=(int)x;float fraction=x-(float)n;
 if(x>=0)return n+(fraction>=0.5f);
 return n+(n<0 && fraction> -0.5f);
}
static float maximum_delay(unsigned k){return maximum_delays[k];}
static int line_length(float rate,unsigned k){return round_half(rate*maximum_delay(k))+16;}
#define MM_ROUND_HALF(x) round_half(x)
#else
static float maximum_delay(unsigned k){return (float)((double)parameters[k][0]+(double)parameters[k][1]*1.125);}
static int line_length(float rate,unsigned k){return (int)((double)(rate*maximum_delay(k))+16.5);}
#define MM_ROUND_HALF(x) ((int)((double)(x)+0.5))
#endif
size_t mm_reverb_samples(float rate){
 if(!isfinite(rate) || rate<8000 || rate>48000)return 0;
 size_t n=0;for(unsigned k=0;k<8;k++)n+=(size_t)line_length(rate,k);return n;
}
static inline __attribute__((always_inline)) void segment(MM_Reverb *s,MM_ReverbLine *p,unsigned k){
 int32_t seed=p->seed;if(seed<0)seed+=65536;
 uint32_t next=(uint32_t)seed*15625u+1u;
 p->seed=(int32_t)(next&65535u);if(next&32768u)p->seed-=65536;
 p->count=MM_ROUND_HALF(s->live_rate/parameters[k][2]);
 float read=fmaf((float)p->fraction,0x1p-28f,(float)p->read);
 float distance=(float)p->write-read;
 while(distance<0)distance+=(float)p->length;
 float previous=distance/s->live_rate;
 float noise=((float)p->seed*parameters[k][1])*0x1p-15f;
 float target=fmaf(s->pitch_mod,noise,parameters[k][0]);
 float difference=(previous-target)/(float)p->count;
 float increment=fmaf(s->live_rate,difference,1.0f)*0x1p28f;
 p->step=MM_ROUND_HALF(increment);
}
int mm_reverb_initialize(MM_Reverb *s,float rate,float *arena,size_t capacity){
 size_t required=mm_reverb_samples(rate);if(!s || !arena || !required || capacity<required)return 0;
 memset(s,0,sizeof *s);s->feedback=0.9700000286102295f;s->cutoff=10000;
 s->sample_rate=s->live_rate=rate;s->pitch_mod=s->damping=1;s->initialized=1;
 size_t offset=0;
 for(unsigned k=0;k<8;k++){
  MM_ReverbLine *p=&s->line[k];p->offset=(uint32_t)offset;p->length=line_length(rate,k);
  p->seed=MM_ROUND_HALF(parameters[k][3]);
  float noise=((float)p->seed*parameters[k][1])*0x1p-15f;
  float target=fmaf(noise,s->pitch_mod,parameters[k][0]);
  float read=fmaf(-target,rate,(float)p->length);p->read=(int)read;
  float fraction=(read-(float)p->read)*0x1p28f;p->fraction=MM_ROUND_HALF(fraction);
  segment(s,p,k);memset(arena+offset,0,(size_t)p->length*sizeof(float));offset+=(size_t)p->length;
 }
 return 1;
}
/* genAMB: the tank is a per-sample feedback loop, so the sample loop cannot be
 * hoisted - but everything around it can. The original entry did four pointer
 * guards, a cutoff comparison and a separate strided pass over all eight lines
 * to re-sum `filtered` on every single sample. The guards and the cutoff check
 * are block-rate facts, and the sum is exactly the value the previous sample
 * already computed line by line. mm_reverb_begin() does the block-rate work
 * once and seeds the running sum; mm_reverb_step() carries it in a register.
 * The accumulation order (k = 0..7, from 0.0f) is unchanged, so the tank is
 * bit-exact. mm_reverb_process() keeps the original one-shot contract. */
PLAT_HOT int mm_reverb_begin(MM_Reverb *s,float *arena,float *carry){
 if(!s || !arena || !carry || s->initialized<=0)return 0;
 if(s->cutoff!=s->previous_cutoff){
  s->previous_cutoff=s->cutoff;
  float x=(s->cutoff*6.2831854820251465f)/s->live_rate;
  float a=2.0f-cosf(x);float b=fmaf(a,a,-1.0f);
  s->damping=a-sqrtf(b);
 }
 float sum=0;for(unsigned k=0;k<8;k++)sum+=s->line[k].filtered;
 *carry=sum;
 return 1;
}
/* One delay line, k resolved by the caller. Returns its new filtered value. */
static inline __attribute__((always_inline)) float mm_reverb_line(
  MM_Reverb *s,float *arena,unsigned k,float input,float feedback,float damping){
 MM_ReverbLine *p=&s->line[k];float *buffer=arena+p->offset;
 const int length=p->length;
 buffer[p->write]=input-p->filtered;
 if(++p->write>=length)p->write-=length;
 int read=p->read,fraction=p->fraction;
 if(fraction>=0x10000000){read+=fraction>>28;fraction=(int32_t)((uint32_t)fraction&0x0fffffffu);p->read=read;}
 if(read>=length){read-=length;p->read=read;}
 float phase=(float)fraction*0x1p-28f;
 float u=fmaf(phase,phase,-1);
#ifdef MM_FM1_FLOAT_ONLY
 u*= 0.1666666716337204f;
#else
 u=(float)((double)u*0.16666666666666666);
#endif
 float plus=phase+1,previous=fmaf(plus,.5f,-1)-u;
 float nextc=fmaf(plus,.5f,-(u*3));
 float current=u*3-phase;
 int i0=read-1;if(i0<0)i0+=length;
 int i2=read+1;if(i2>=length)i2-=length;
 int i3=i2+1;if(i3>=length)i3-=length;
 const float centre=buffer[read];
 float v=centre*current;v=fmaf(buffer[i0],previous,v);v=fmaf(buffer[i2],nextc,v);v=fmaf(buffer[i3],u,v);v=fmaf(v,phase,centre);
 p->fraction=(int32_t)((uint32_t)fraction+(uint32_t)p->step);
 float filtered=fmaf(-feedback,v,p->filtered);filtered*=damping;filtered=fmaf(feedback,v,filtered);p->filtered=filtered;
 if(--p->count<=0)segment(s,p,k);
 return filtered;
}
/* genAMB: the lines alternate between the two inputs and the two outputs, so
 * the original indexed a pair of two-element arrays by k&1 on every line of
 * every sample - and because the index is a variable, both arrays live on the
 * stack and every line pays a load and a load-modify-store for them. Stepping
 * the loop two lines at a time makes that parity a compile-time fact and lets
 * both output sums stay in registers. The lines are still visited 0..7 in
 * order, so the feedback sum and both output sums are accumulated in exactly
 * the original order, and both forms below are bit-exact to each other.
 *
 * The pair form is worth 1.5% of the whole audio path in host instruction
 * count, at about 2.4 kB more code: all eight lines run every sample, so the
 * instruction footprint the loop touches per sample roughly doubles. That is
 * a good trade wherever instruction fetch is cheap, and this part executes in
 * place from the flash over XIP. Build with -DGENAMB_REVERB_PAIRS=0 to get
 * the single-body loop back and compare the two on the unit. */
/* Default 0: measured on the part, the pair form buys 0.4% of the tank for
 * 1,232 bytes and twice the hot-loop footprint. Build -DGENAMB_REVERB_PAIRS=1
 * to compare. */
#ifndef GENAMB_REVERB_PAIRS
#define GENAMB_REVERB_PAIRS 0
#endif
static inline __attribute__((always_inline)) void mm_reverb_step(
  MM_Reverb *s,float *arena,float left,float right,float out[2],float *carry){
 const float feedback=s->feedback,damping=s->damping;
 const float sum=*carry;
 const float in0=fmaf(sum,.25f,left),in1=fmaf(sum,.25f,right);
 float out0=0,out1=0,next=0;
#if GENAMB_REVERB_PAIRS
 for(unsigned k=0;k<8;k+=2){
  float fa=mm_reverb_line(s,arena,k,in0,feedback,damping);
  next+=fa;out0+=fa;
  float fb=mm_reverb_line(s,arena,k+1u,in1,feedback,damping);
  next+=fb;out1+=fb;
 }
#else
 {
  const float inputs[2]={in0,in1};
  float outputs[2]={0,0};
  for(unsigned k=0;k<8;k++){
   float f=mm_reverb_line(s,arena,k,inputs[k&1u],feedback,damping);
   next+=f;outputs[k&1u]+=f;
  }
  out0=outputs[0];out1=outputs[1];
 }
#endif
 *carry=next;
 out[0]=out0*0.3499999940395355f;out[1]=out1*0.3499999940395355f;
}
/* Lean per-sample entry for a prepared block. */
PLAT_HOT void mm_reverb_sample(MM_Reverb *s,float *arena,float left,float right,float out[2],float *carry){
 mm_reverb_step(s,arena,left,right,out,carry);
}
PLAT_HOT int mm_reverb_process(MM_Reverb *s,float *arena,float left,float right,float out[2]){
 float carry;
 if(!out || !mm_reverb_begin(s,arena,&carry))return 0;
 mm_reverb_step(s,arena,left,right,out,&carry);
 return 1;
}
