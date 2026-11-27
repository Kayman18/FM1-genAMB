/* Run each comparison in a fresh process with the production DSP/mixer. */
#include "amb.h"
#include "fm_port.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
uint32_t host_perf_dual=1;
static unsigned notes;
void en_midi_out(uint32_t s,uint32_t a,uint32_t b){(void)a;if(s==0x92 && b)notes++;}
uint32_t plat_ms(void){return 0;}
uint32_t plat_lock(void){return 0;}
void plat_unlock(uint32_t f){(void)f;}
void plat_log(const char *s){(void)s;}

int main(int argc,char **argv)
{
    assert(argc==4);
    int preset=atoi(argv[1]),dest=atoi(argv[2]),route=atoi(argv[3]);
    int16_t params[P_NPARAMS];
    uint16_t pool;
    int32_t pcm[256];
    en_init();
    en_piece(0,params,&pool);
    va_preset_load(preset,params);
    en_set(P_PAD,0);en_set(P_PULSE,0);en_set(P_BELLS,0);
    en_set(P_REVERB,0);en_set(P_FMLEVEL,78);
    const int shape[]={P_M7SHAPE,P_M8SHAPE,P_M9SHAPE,P_M12SHAPE,P_M13SHAPE,P_M14SHAPE};
    for(int i=0;i<6;i++){
        en_set(lfo_destination_id[i],route==i?dest:0);
        en_set(lfo_depth_id[i],35);en_set(shape[i],3);
    }
    if(route>=6){
        en_set(P_FMSEQMODE,route-6);en_set(P_TEMPO,120);
        en_set(P_FMDENSITY,100);en_set(P_FMEDENSITY,100);
        en_set(P_FPROTDENS,100);en_set(P_FUDGDEN,100);
        en_play(1);
        int32_t peak=0;
        for(int i=0;i<8300;i++){
            en_render(pcm,128,4096);
            for(int j=0;j<256;j++)if(pcm[j]>peak)peak=pcm[j];
        }
        assert(notes && peak>1000);
        printf("sequencer %d, %u triggers, peak %d\n",route-6,notes,peak);
        return 0;
    }
    en_render(pcm,128,4096);
    for(int i=0;i<4;i++)en_fm_note(38+i*5,105);
    uint64_t hash=1469598103934665603ull;
    uint32_t peak=0;
    for(int block=0;block<2900;block++){
        if(block==200)en_fm_all_notes_off();
        en_render(pcm,128,4096);
        if(block==0)assert(fm_port_active_voices==4);
        for(int i=0;i<256;i++){
            unsigned mag=abs(pcm[i]);
            assert(mag<8388607);
            if(mag>peak)peak=mag;
            if(block<800)hash=(hash^(uint32_t)pcm[i])*1099511628211ull;
        }
    }
    assert(peak>1000 && fm_port_active_voices==0);
    printf("%016llx\n",(unsigned long long)hash);
}
