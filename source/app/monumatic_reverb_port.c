/* TEST1 replacement: same recovered kernel as hardware-accepted Monumatic TEST22.
 * Mono genAMB send feeds both inputs; wet-only output retains genAMB's mix law. */
#include "monumatic/reverb.h"
#include "plat.h"
#include "monumatic_reverb_port.h"
#include "fastmath.h"
#if defined(PLAT_HOST) || defined(EN_HOST)
#define REV_POOL
#else
#define REV_POOL __attribute__((section(".pool")))
#endif
#define SAMPLES 22729u
static float arena[SAMPLES] REV_POOL;
static MM_Reverb state REV_POOL,initial REV_POOL;
static uint32_t clear;
static float previous,highpass,wet_gain;
void monumatic_reverb_init(void){
 mm_reverb_initialize(&state,44100,arena,SAMPLES);initial=state;
 clear=SAMPLES;previous=highpass=wet_gain=0;
}
void monumatic_reverb_panic(void){
 state=initial;clear=0;previous=highpass=wet_gain=0;
}
void monumatic_reverb_set(float decay,float tone,float motion){
 state.feedback=.7f+.27f*fm_clampf(decay,0,1);
 /* Tone85 = TEST22's 10 kHz damping; Tone100 = 13.8 kHz. */
 tone=fm_clampf(tone,0,1);
 state.cutoff=100.0f+tone*tone*(9900.0f/(.85f*.85f));
 state.pitch_mod=fm_clampf(motion,0,1);
}
/* genAMB: the tank's per-sample guards, cutoff test and eight-line feedback
 * re-sum are block-rate work; mm_reverb_begin() does them once and hands back
 * the running sum, which mm_reverb_sample() then carries. The arena clear and
 * the live tank never share a sample, so the clear is peeled out of the hot
 * loop instead of being tested on every one. highpass/previous/wet_gain are
 * held in locals for the block - nothing observes them mid-block. */
PLAT_HOT void monumatic_reverb_process(float *mono,float *right,uint32_t n){
 uint32_t i=0;
 for(;i<n && clear<SAMPLES;i++){
  uint32_t end=clear+16;if(end>SAMPLES)end=SAMPLES;
  while(clear<end)arena[clear++]=0;
  mono[i]=right[i]=0;
 }
 if(i>=n)return;
 float carry;
 if(!mm_reverb_begin(&state,arena,&carry)){
  /* Unusable state: keep the original one-shot path and its observable
   * behaviour rather than inventing output for it. */
  for(;i<n;i++){
   float input=mono[i],wet[2];
   highpass=input-previous+.99715453f*highpass;previous=input;
   mm_reverb_process(&state,arena,highpass,highpass,wet);
   wet_gain+=(1.0f-wet_gain)*.004f;
   mono[i]=wet[0]*wet_gain;right[i]=wet[1]*wet_gain;
  }
  return;
 }
 float hp=highpass,pv=previous,wg=wet_gain;
 for(;i<n;i++){
  float input=mono[i],wet[2];
  hp=input-pv+.99715453f*hp;pv=input;
  mm_reverb_sample(&state,arena,hp,hp,wet,&carry);
  wg+=(1.0f-wg)*.004f;
  mono[i]=wet[0]*wg;right[i]=wet[1]*wg;
 }
 highpass=hp;previous=pv;wet_gain=wg;
}
