/* Full-engine solo FM probe. Each invocation starts from a fresh state. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amb.h"

uint32_t host_perf_dual=1;
void en_midi_out(uint32_t s,uint32_t a,uint32_t b){(void)s;(void)a;(void)b;}
uint32_t plat_ms(void){return 0;}
uint32_t plat_lock(void){return 0;}
void plat_unlock(uint32_t flags){(void)flags;}
void plat_log(const char *s){(void)s;}

int main(int argc,char **argv)
{
    if(argc!=5 && argc!=6 && argc!=8)return 2;
    const int destination=argc==8 ? atoi(argv[5]) : 15;
    const int cycling = argc==6 && !strcmp(argv[5],"cycle");
    const int dest[3]={P_M7DEST,P_M8DEST,P_M9DEST};
    const int depth[3]={P_M7DEPTH,P_M8DEPTH,P_M9DEPTH};
    const int shape[3]={P_M7SHAPE,P_M8SHAPE,P_M9SHAPE};
    const int speed[3]={P_M7SPEED,P_M8SPEED,P_M9SPEED};
    const int base=atoi(argv[1]);
    en_init(); en_set(P_PAD,0); en_set(P_PULSE,0); en_set(P_BELLS,0);
    en_set(P_REVERB,0);en_set(P_NATURE_MASTER,0);
    en_set(P_FMPATCH,0);en_set(P_FMLEVEL,base);
    if(argc==8) { en_set(P_FMDEPTH,atoi(argv[6])); en_set(P_FMALGO,atoi(argv[7])); }
    for(unsigned i=0;i<3;++i){
        en_set(dest[i],atoi(argv[2+i])!=0?destination:0); en_set(depth[i],atoi(argv[2+i]));
        en_set(shape[i],3);en_set(speed[i],cycling ? 100 : 1);
    }
    if(argc==8) { int32_t pre[256]; en_render(pre,128,4096); }
    en_fm_note(60,102);
    uint64_t h=1469598103934665603ull;
    double energy=0.0,peak=0.0;
    double windows[3]={0,0,0};
    int blocks=cycling ? 5600 : 72;
    for(int block=0;block<blocks;++block){
        if (cycling && block%64==0) en_fm_note(60,102);
        int32_t pcm[256];
        en_render(pcm,128,4096);
        for(unsigned j=0;j<256;++j) {
            double x=(double)pcm[j]/8388607.0;
            h=(h^(uint32_t)pcm[j])*1099511628211ull;
            energy+=x*x;
            if(block>=320 && block<384)windows[0]+=x*x;
            if(block>=1700 && block<1764)windows[1]+=x*x;
            if(block>=5440 && block<5504)windows[2]+=x*x;
            if(fabs(x)>peak)peak=fabs(x);
        }
    }
    if(cycling) { printf("%.12g %.12g %.12g\n",windows[0],windows[1],windows[2]);return 0; }
    printf("%016llx %.12f %.8f\n",(unsigned long long)h,sqrt(energy/(72*256)),peak);
    return 0;
}
