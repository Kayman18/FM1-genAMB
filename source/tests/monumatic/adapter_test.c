#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "monumatic_reverb_port.h"
static float l[257],r[257];
static unsigned frames;static double energy;static float peak;
static void block(unsigned n,int silent){
 monumatic_reverb_process(l,r,n);
 for(unsigned i=0;i<n;i++){
  assert(isfinite(l[i]) && isfinite(r[i]));
  if(silent)assert(l[i]==0 && r[i]==0);
  energy+=(double)l[i]*l[i]+(double)r[i]*r[i];
  float p=fmaxf(fabsf(l[i]),fabsf(r[i]));if(p>peak)peak=p;
  assert(p<2.0f);frames++;
 }
}
static void zero(unsigned n){for(unsigned i=0;i<n;i++)l[i]=r[i]=0;}
int main(void){
 monumatic_reverb_init();monumatic_reverb_set(1,.85f,1);
 for(unsigned i=0;i<500;i++){zero(257);block(257,1);}
 for(unsigned i=0;i<12000;i++){
  unsigned n=1+(i*97u)%257u;
  zero(n);if(i%500==0)l[0]=.125f;
  if(i%193==0)monumatic_reverb_set((float)(i%101)*.01f,(float)((i*3)%101)*.01f,(float)((i*7)%101)*.01f);
  block(n,0);
 }
 assert(energy>.001);
 /* Panic removes all audible history, including after the incremental clear. */
 monumatic_reverb_panic();monumatic_reverb_set(1,1,1);
 for(unsigned i=0;i<1000;i++){zero(257);block(257,1);}
 /* Maximum-decay minute with intermittent excitation and control changes. */
 for(unsigned i=0;i<10300;i++){
  zero(257);if(i%400==0)l[0]=.125f;block(257,0);
 }
 printf("PASS frames=%u peak=%g energy=%g: initial silence, varying blocks/controls, max decay, panic and stale-history clear\n",frames,peak,energy);
}
