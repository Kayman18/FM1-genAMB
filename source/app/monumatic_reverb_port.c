/* TEST1 replacement: same recovered kernel as hardware-accepted Monumatic TEST22.
 * Mono genAMB send feeds both inputs; wet-only output retains genAMB's mix law. */
#include "monumatic/reverb.h"
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
void monumatic_reverb_process(float *mono,float *right,uint32_t n){
 for(uint32_t i=0;i<n;i++){
  if(clear<SAMPLES){
   uint32_t end=clear+16;if(end>SAMPLES)end=SAMPLES;
   while(clear<end)arena[clear++]=0;
   mono[i]=right[i]=0;continue;
  }
  float input=mono[i],wet[2];
  highpass=input-previous+.99715453f*highpass;previous=input;
  mm_reverb_process(&state,arena,highpass,highpass,wet);
  wet_gain+=(1.0f-wet_gain)*.004f;
  mono[i]=wet[0]*wet_gain;right[i]=wet[1]*wet_gain;
 }
}
