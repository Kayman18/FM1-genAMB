/* The reported M8 volume + M9 algorithm pairing through the full audio engine. */
#include "amb.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

uint32_t host_perf_dual=1;
static unsigned triggers;
void en_midi_out(uint32_t s,uint32_t note,uint32_t vel)
{
    (void)note;
    if(s==0x92 && vel) ++triggers;
}
uint32_t plat_ms(void){return 0;}
uint32_t plat_lock(void){return 0;}
void plat_unlock(uint32_t flags){(void)flags;}
void plat_log(const char *s){(void)s;}

static void solo(int base)
{
    en_init();
    en_set(P_PAD,0); en_set(P_PULSE,0); en_set(P_BELLS,0);
    en_set(P_REVERB,0); en_set(P_NATURE_MASTER,0);
    en_set(P_FMPATCH,15); en_set(P_FMLEVEL,base); en_set(P_FMALGO,17);
    en_set(P_M8DEST,DEST_FM_VOLUME); en_set(P_M8DEPTH,100);
    en_set(P_M8SHAPE,2); en_set(P_M8SPEED,109); /* 2 s sine */
    en_set(P_M9DEST,DEST_FM_ALGO); en_set(P_M9DEPTH,100);
    en_set(P_M9SHAPE,3); en_set(P_M9SPEED,106); /* 4 s pulse */
}

int main(int argc,char **argv)
{
    if (argc>1 && !strcmp(argv[1],"tone")) {
        static int32_t first[8192];
        double x2=0, y2=0, xy=0;
        for (int pass=0; pass<2; ++pass) {
            int32_t pcm[256];
            solo(50);
            en_set(P_M8DEPTH,0);
            en_set(P_M9DEPTH,pass ? -100 : 100);
            en_render(pcm,128,4096);
            en_fm_note(60,100);
            for (int k=0; k<8192; k+=128) {
                en_render(pcm,128,4096);
                for (int j=0;j<128;++j) {
                    if (!pass) first[k+j]=pcm[2*j];
                    else {
                        double x=first[k+j], y=pcm[2*j];
                        xy+=x*y; x2+=x*x; y2+=y*y;
                    }
                }
            }
            assert(en_fm_last_algo==(pass ? 1u : 32u));
        }
        double corr=xy/sqrt(x2*y2);
        printf("Reso Bass MIDI 60 velocity 100, M9 selects 32 vs 1 at FM level 50: normalized waveform correlation %.5f\n",corr);
        assert(fabs(corr)<.85);
        return 0;
    }
    if (argc>2 && !strcmp(argv[1],"dest")) {
        int dest=atoi(argv[2]);
        en_init(); en_set(P_FMLEVEL,50); en_set(P_FMALGO,17);
        en_set(P_M7DEST,0); en_set(P_M8DEST,dest); en_set(P_M9DEST,0);
        en_set(P_M8SHAPE,2); en_set(P_M8SPEED,109); en_set(P_M8DEPTH,100);
        en_set(P_RMODEL,1); en_set(P_BELLS,65); en_set(P_PADTONE,1);
        en_set(P_FMPATCH,15); en_set(P_REVERB,50); en_play(1);
        uint64_t hash=1469598103934665603ull;
        for(int block=0;block<1700;++block) {
            if(block%83==0){en_fm_note(60,100);en_rings_note(60,100);}
            int32_t pcm[256];en_render(pcm,128,4096);
            for(int j=0;j<256;++j)hash=(hash^(uint32_t)pcm[j])*1099511628211ull;
        }
        printf("%016llx\n",(unsigned long long)hash);
        return 0;
    }
    if (argc>1 && !strcmp(argv[1],"random")) {
        int32_t pcm[256];
        solo(50);
        en_set(P_M8SHAPE,0); en_set(P_M8SPEED,0);
        en_render(pcm,128,4096);
        float start=en_fm_volume_target;
        en_set(P_M8SPEED,115);
        /* The new target is first sampled on a one-frame sequencer split. */
        en_render(pcm,1,4096);
        float low=100, high=0;
        for (int i=0;i<600;++i) {
            en_render(pcm,128,4096);
            float level=en_fm_volume_target;
            if(level<low)low=level;
            if(level>high)high=level;
        }
        printf("Smooth random after changing 20 min to 250 ms: %.2f..%.2f (start %.3f)\n",low,high,start);
        assert(high-low>10);
        return 0;
    }
    const int mode=argc>1 ? atoi(argv[1]) : -1;
    if(mode>=0) {
        solo(0);
        en_set(P_FMSEQMODE,mode); en_set(P_TEMPO,120);
        en_set(P_FMDENSITY,100); en_set(P_FMEDENSITY,100);
        en_set(P_FPROTDENS,100); en_set(P_FUDGDEN,100);
        en_play(1);
    } else solo(50);
    int32_t pcm[256];
    double energy=0,hi=0,lo=100;
    unsigned mask=0;
    int blocks=(mode>=0 ? 24 : 8)*44100/128;
    for(int i=0;i<blocks;++i) {
        if(mode<0 && i%65==1) en_fm_note(60,110);
        en_render(pcm,128,4096);
        double level=en_fm_volume_target;
        if(level<lo)lo=level;
        if(level>hi)hi=level;
        unsigned algo=en_fm_last_algo;
        if(algo) mask |= 1u<<(algo-1);
        for(int j=0;j<256;++j){double x=pcm[j]*(1./8388608.);energy+=x*x;}
    }
    assert(energy>1 && triggers>0);
    assert(hi>=(mode<0?94.9:44.9) && lo<=(mode<0?5.1:0.01));
    printf("mode %d, %u triggers, volume %.2f..%.2f, algo mask %08x, PCM RMS %.6f\n",
           mode,triggers,lo,hi,mask,sqrt(energy/(256*blocks)));
    if(mode<0) assert(mask==0x80000001u);
    else assert(mask & 0x80000001u);
    return 0;
}
