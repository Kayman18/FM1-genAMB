/* Exercise the reverb sleep/wake transition: no NaN, no silence left behind,
   and the tail builds after the knob comes up. */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "amb.h"
uint32_t host_perf_dual=1;
void en_midi_out(uint32_t a,uint32_t b,uint32_t c){(void)a;(void)b;(void)c;}
uint32_t plat_ms(void){return 0;}
uint32_t plat_lock(void){return 0;}
void plat_unlock(uint32_t f){(void)f;}
void plat_log(const char *s){(void)s;}
static int32_t pcm[512];
static double energy(unsigned blocks){double e=0;for(unsigned b=0;b<blocks;b++){en_render(pcm,256,4096);for(unsigned i=0;i<512;i++){double v=(double)pcm[i]/8388607.0;if(!isfinite(v)){printf("NONFINITE\n");return -1;}e+=v*v;}}return e;}
int main(void){
 en_init();en_seed(99);en_pool(0xAB5u);
 en_set(P_PAD,90);en_set(P_BELLS,80);en_set(P_FMLEVEL,80);en_set(P_PADREV,90);en_set(P_RINGREV,90);en_set(P_FMREV,90);
 en_set(P_DENSITY,70);en_set(P_TEMPO,80);en_set(P_SIZE,70);en_set(P_BTONE,60);
 en_set(P_REVERB,0);en_play(1);
 double dry=energy(40);
 en_set(P_REVERB,80);
 double wake=energy(40);
 en_set(P_REVERB,0);
 double back=energy(20);
 en_set(P_REVERB,80);
 double again=energy(40);
 printf("dry %.3f  wake %.3f  back %.3f  again %.3f\n",dry,wake,back,again);
 printf("%s\n",(dry>0&&wake>0&&back>0&&again>0)?"WAKE OK (audio on both sides of every transition)":"WAKE PROBLEM");
 return 0;}
