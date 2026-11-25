#define _POSIX_C_SOURCE 200809L
#include <time.h>
#ifdef __SSE__
#include <xmmintrin.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "amb.h"
uint32_t host_perf_dual=1;
void en_midi_out(uint32_t s,uint32_t a,uint32_t b){(void)s;(void)a;(void)b;}
uint32_t plat_ms(void){return 0;}
uint32_t plat_lock(void){return 0;}
void plat_unlock(uint32_t f){(void)f;}
void plat_log(const char *s){(void)s;}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc,char **argv){
    if(argc<2)return 2;
    int transitions = argc==3 && !strcmp(argv[2],"transition-pcm");
    int pcm_output = transitions || (argc==3 && !strcmp(argv[2],"pcm"));
    if (argc>2 && !pcm_output) return 2;
#ifdef __SSE__
    _mm_setcsr(_mm_getcsr() | 0x8040u); /* Timing only: flush x86 denormals. */
#endif
    int scene=atoi(argv[1]); int32_t pcm[512]; uint32_t h=0;
    en_init(); en_set(P_RMODEL,scene==4?1:scene==3?2:0); en_set(P_RPOLY,4);
    en_set(P_FMPOLY,6);en_set(P_FMLEVEL,85);en_set(P_BELLS,80);
    en_set(P_PAD,80);en_set(P_PULSE,80);en_set(P_REVERB,65);
    en_set(P_DENSITY,70);en_set(P_MOTION,50);en_set(P_TEMPO,80);
    en_set(P_NATURE_MASTER,scene==2?0:80);en_set(P_NATURE_WATER,80);
    en_set(P_NATURE_RAIN,80);en_set(P_NATURE_ANIMALS,65);en_set(P_NATURE_DENSITY,100);
    en_set(P_RDELAYMIX,scene==1?70:0);
    en_seed(4213);en_play(1);
    if (scene>=5 && scene<=7) {
        en_set(P_FMPATCH,15); en_set(P_FMALGO,17);
        en_set(P_M8DEST,15); en_set(P_M8SHAPE,2); en_set(P_M8SPEED,109); en_set(P_M8DEPTH,100);
        en_set(P_M9DEST,17); en_set(P_M9SHAPE,3); en_set(P_M9SPEED,106); en_set(P_M9DEPTH,100);
        if (scene==5) {
            en_set(P_PADTONE,1);
        } else {
            en_set(P_PAD,0); en_set(P_PULSE,0); en_set(P_BELLS,0);
            en_set(P_M7DEST,0); en_set(P_RMODEL,scene==7 ? 1 : 0);
        }
    }
    double start=0;
    for(int b=0;b<8800;++b) {
        if (b==800) start=seconds();
        if (b%113==0){int note=48+((b/113)%18);en_rings_note(note,103);en_fm_note(note+12,102);}
        if(b%113==75)en_fm_note_off(60+((b/113)%18));
        if (transitions) {
            switch (b) {
            case 900: en_set(P_BELLS,0); en_set(P_PAD,0); en_set(P_PULSE,0); break;
            case 1200: en_set(P_RMODEL,1); en_set(P_FMPATCH,6); en_set(P_TUNE,30); break;
            case 1800: en_set(P_BELLS,72); en_set(P_PAD,80); en_set(P_PULSE,80); break;
            case 2100: en_set(P_TUNE,-25); en_set(P_FMPATCH,0); en_set(P_PADTONE,1); break;
            case 2500: en_set(P_RMODEL,0); en_set(P_REVERB,0); break;
            case 2800: en_set(P_REVERB,80); en_set(P_FMPATCH,15); en_set(P_FMDEPTH,62); break;
            case 3200: en_play(0); break;
            case 3700: en_play(1); break;
            }
        }
        en_render(pcm,256,4096);
        if (pcm_output) fwrite(pcm,sizeof(pcm),1,stdout);
        h=h*31+(uint32_t)pcm[197];
    }
    if (!pcm_output) printf("%.6f %08x\n",seconds()-start,h);
}
