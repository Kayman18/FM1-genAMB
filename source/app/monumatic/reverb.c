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
PLAT_HOT int mm_reverb_process(MM_Reverb *s,float *arena,float left,float right,float out[2]){
 if(!s || !arena || !out || s->initialized<=0)return 0;
 if(s->cutoff!=s->previous_cutoff){
  s->previous_cutoff=s->cutoff;
  float x=(s->cutoff*6.2831854820251465f)/s->live_rate;
  float a=2.0f-cosf(x);float b=fmaf(a,a,-1.0f);
  s->damping=a-sqrtf(b);
 }
 float sum=0;for(unsigned k=0;k<8;k++)sum+=s->line[k].filtered;
 float inputs[2]={fmaf(sum,.25f,left),fmaf(sum,.25f,right)};
 float outputs[2]={0,0};
 for(unsigned k=0;k<8;k++){
  MM_ReverbLine *p=&s->line[k];float *buffer=arena+p->offset;
  buffer[p->write]=inputs[k&1u]-p->filtered;
  if(++p->write>=p->length)p->write-=p->length;
  int read=p->read,fraction=p->fraction;
  if(fraction>=0x10000000){read+=fraction>>28;fraction=(int32_t)((uint32_t)fraction&0x0fffffffu);p->read=read;}
  if(read>=p->length){read-=p->length;p->read=read;}
  float phase=(float)fraction*0x1p-28f;
  float u=fmaf(phase,phase,-1);
#ifdef MM_FM1_FLOAT_ONLY
  u*= 0.1666666716337204f;
#else
  u=(float)((double)u*0.16666666666666666);
#endif
  float plus=phase+1,previous=fmaf(plus,.5f,-1)-u;
  float next=fmaf(plus,.5f,-(u*3));
  float current=u*3-phase;
  int i0=read-1;if(i0<0)i0+=p->length;
  int i2=read+1;if(i2>=p->length)i2-=p->length;
  int i3=i2+1;if(i3>=p->length)i3-=p->length;
  float v=buffer[read]*current;v=fmaf(buffer[i0],previous,v);v=fmaf(buffer[i2],next,v);v=fmaf(buffer[i3],u,v);v=fmaf(v,phase,buffer[read]);
  p->fraction=(int32_t)((uint32_t)fraction+(uint32_t)p->step);
  float filtered=fmaf(-s->feedback,v,p->filtered);filtered*=s->damping;filtered=fmaf(s->feedback,v,filtered);p->filtered=filtered;
  outputs[k&1u]+=filtered;
  if(--p->count<=0)segment(s,p,k);
 }
 out[0]=outputs[0]*0.3499999940395355f;out[1]=outputs[1]*0.3499999940395355f;
 return 1;
}
