#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "fm_filter.h"
void ref_init(void);void ref_set(int,int,int,int,int,float,float);float ref_sample(float);
int main(void){
 fm_filter_state_t f,a,b;unsigned samples=0;
 for(int m=0;m<6;m++)for(int type=0;type<4;type++)for(int c=0;c<=100;c+=50)for(int r=0;r<=100;r+=50){
  ref_init();classic_filter_init(&f);
  ref_set(m,type,c,r,100,0,0);classic_filter_set(&f,m,type,c,r,100,0,0);
  for(int i=0;i<4096;i++){
   float x=.4f*sinf(i*.073f),y=classic_filter_sample(&f,x),z=ref_sample(x);
   assert(isfinite(y) && fabsf(y-z)<0.000001f);samples++;
  }
 }
 classic_filter_init(&a);classic_filter_init(&b);classic_filter_set(&a,1,0,60,25,100,0,0);classic_filter_set(&b,1,0,60,25,100,0,0);
 for(int i=0;i<8192;i++){classic_filter_sample(&a,i<32?.1f:0.f);assert(classic_filter_sample(&b,0.f)==0.f);}
 printf("PASS %u reference samples: all six models, four types, cutoff/resonance extremes; independent stereo histories\n",samples);
}
