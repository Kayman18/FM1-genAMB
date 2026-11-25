#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "amb.h"
#include "nature.h"
uint32_t host_perf_dual=1;
void en_midi_out(uint32_t st,uint32_t d1,uint32_t d2){(void)st;(void)d1;(void)d2;}
uint32_t plat_ms(void){return 0;}
uint32_t plat_lock(void){return 0;}
void plat_unlock(uint32_t f){(void)f;}
void plat_log(const char *s){(void)s;}

static int32_t pcm[512];
static unsigned render(unsigned frames, unsigned gain)
{
    unsigned nonzero=0;
    en_render(pcm,frames,gain);
    for(unsigned j=0;j<2*frames;++j) {
        assert(pcm[j]>=-8388607 && pcm[j]<=8388607);
        nonzero+=pcm[j]!=0;
    }
    return nonzero;
}
static void stop_river(int midi_stop)
{
    if (midi_stop) en_clock(EN_CLK_STOP); else en_play(0);
    for (int k=0;k<8;++k) render(256,4096);
    render(156,4096);
    assert(NATURE_WATER_ON(nature_status));
    assert(render(1,4096)==0 && !NATURE_WATER_ON(nature_status));
    assert(render(256,4096)==0);
}
static uint64_t river_run(int reverb, int legacy_delay)
{
    en_init();
    en_set(P_PAD,0); en_set(P_PULSE,0); en_set(P_BELLS,0); en_set(P_FMLEVEL,0);
    en_set(P_REVERB,reverb); en_set(P_NATURE_WATER,100); en_set(P_RDELAYMIX,legacy_delay);
    en_set(P_NATURE_RAIN,0); en_set(P_NATURE_ANIMALS,0); en_set(P_NATURE_MASTER,100);
    for(unsigned i=0;i<12;++i) assert(render(256,4096)==0);
    en_play(1);
    uint64_t h=1469598103934665603ull;
    for(unsigned block=0;block<1800;++block){
        assert(render(256,4096)>300);
        for(unsigned i=0;i<512;++i) h=(h^(uint32_t)pcm[i])*1099511628211ull;
    }
    stop_river(0);
    en_set(P_NATURE_WATER,72); assert(render(256,4096)==0);
    en_clock(EN_CLK_START); assert(render(256,4096)>300);
    stop_river(1);
    en_clock(EN_CLK_CONTINUE); assert(render(256,4096)>300);
    assert(render(256,0)==0);
    en_panic(); en_set(P_SIZE,85); assert(render(256,4096)==0);
    en_set(P_NATURE_MASTER,80);assert(render(256,4096)>300);
    return h;
}
static void test_animal_transport(int external)
{
    en_init(); en_set(P_PAD,0); en_set(P_PULSE,0); en_set(P_BELLS,0); en_set(P_FMLEVEL,0);
    en_set(P_REVERB,0); en_set(P_NATURE_MASTER,100); en_set(P_NATURE_WATER,0);
    en_set(P_NATURE_RAIN,0); en_set(P_NATURE_ANIMALS,100); en_set(P_NATURE_DENSITY,100);
    for(int i=0;i<1500;++i) assert(render(256,4096)==0 && !NATURE_BIRDS(nature_status));
    if(external) en_clock(EN_CLK_START); else en_play(1);
    unsigned i=0;
    while(NATURE_BIRDS(nature_status)<2 && i++<40000) render(256,4096);
    assert(NATURE_BIRDS(nature_status)==2);
    if(external) en_clock(EN_CLK_STOP); else en_play(0);
    for(i=0;i<8;++i) render(256,4096);
    render(156,4096); assert(NATURE_BIRDS(nature_status)>0);
    assert(render(1,4096)==0 && !NATURE_BIRDS(nature_status));
    en_set(P_NATURE_ANIMALS,64); en_set(P_NATURE_DENSITY,99);
    for(i=0;i<1800;++i) assert(render(256,4096)==0 && !NATURE_BIRDS(nature_status));
    en_set(P_NATURE_RAIN,100);
    unsigned rain=0;
    for(i=0;i<1800;++i) {rain+=render(256,4096);assert(!NATURE_BIRDS(nature_status));}
    assert(rain && NATURE_RAIN_PHASE(nature_status)==NATURE_FADE_IN);
    if(external) en_clock(EN_CLK_CONTINUE); else en_play(1);
    for(i=0;i<1600 && !NATURE_BIRDS(nature_status);++i) render(256,4096);
    assert(NATURE_BIRDS(nature_status)>0);
}
int main(void)
{
    uint64_t dry=river_run(0,0),wet=river_run(100,100);
    assert(dry==wet);
    static uint8_t bank[ANIMAL_WAVE_END-ANIMAL_WAVE_BASE];
    FILE *f=fopen("assets/nature/animal-wave.bin","rb");
    assert(f && fread(bank,1,sizeof bank,f)==sizeof bank);fclose(f);
    en_nature_mount(bank,sizeof bank);assert(en_nature_calls()==10);
    test_animal_transport(0);test_animal_transport(1);
    puts("PASS active animal overlaps fade to silence in 50 ms on local and MIDI Stop, no new calls in REST, Rain continues, PLAY/Continue restart Animals");
}
