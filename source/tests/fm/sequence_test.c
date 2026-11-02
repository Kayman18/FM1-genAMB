/* SPDX-License-Identifier: MPL-2.0
 * Native tests of the production FM sequencer, using only mocked voice/MIDI sinks.
 * The source under test is included below; the harness does not copy it.
 * Run ./tests/fm/run_sequence_test.sh [--sanitize] with a C11 compiler.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../app/dsp/amb.h"
#include "../../app/fm_port/fm_port.h"
#define BARRIER() __asm__ __volatile__("" ::: "memory")
static int16_t par[P_NPARAMS];
static int nsteps=7,steps[12]={0,2,4,5,7,9,11},deg;
static int chord_nt=4,chord_pcs[4]={0,4,7,11};
static float evolve=0.35f, arcm=0.0f,step_len=5512.5f;
volatile uint8_t en_ext;
const uint8_t EN_LOOP_LEN[EN_NLOOP]={13,17,23,29};
static const uint8_t LOOP_BASE[EN_NLOOP]={72,67,79,62};
static float fm_clampf(float f,float lo,float hi){return f<lo?lo:f>hi?hi:f;}
static float fm_maxf(float a,float b){return a>b?a:b;}
static int seq_note(int note,int range,int chance,float roll,float direction){return note+((!range||chance<=0||roll>=chance*0.01f)?0:range==1||direction>=0.5f?12:-12);}
static int triggers, midi_on, midi_off, last_note;static uint32_t last_gate;
static int active[128];
static void record_tick(void);
void fm_port_trigger(int note,int velocity,uint32_t gate){assert(note>=0&&note<=127);assert(velocity>=1&&velocity<=127);assert(gate>=1);triggers++;if(en_fm_evo_active)record_tick();last_note=note;last_gate=gate;}
void en_midi_out(uint32_t st,uint32_t note,uint32_t velocity){assert(note<128);if(st==0x92){assert(velocity>0);midi_on++;active[note]++;}else if(st==0x82){assert(active[note]>0);midi_off++;active[note]--;}else assert(0);}
/* Proteus is provided by amb.c after fm_sequence.c in the release unity
 * translation unit; the older isolated FM harness stubs that sibling. */
static uint32_t prot_gate_samples(void){return 1000u;}
static uint32_t ud_gate_samples(void){return 1000u;}
static int capture_ticks, tick_count, test_clock;
static int tick_time[1024];
static void record_tick(void){if(capture_ticks){assert(tick_count<1024);tick_time[tick_count++]=test_clock;}}
static void ud_tick(int layer){(void)layer;record_tick();}
static void prot_tick(int layer){(void)layer;record_tick();}
#include "../../app/dsp/fm_sequence.c"
static void init(void){
    memset(par,0,sizeof par);memset(active,0,sizeof active);en_ext=0;triggers=midi_on=midi_off=0;
    nsteps=7;int base[]={0,2,4,5,7,9,11};memcpy(steps,base,sizeof base);deg=0;chord_nt=4;
    int ch[]={0,4,7,11};memcpy(chord_pcs,ch,sizeof ch);step_len=5512.5f;
    par[P_FMPOLY]=4;par[P_FMGATE]=75;par[P_FMDENSITY]=70;par[P_FMELENGTH]=16;
    par[P_FMEDENSITY]=100;par[P_FMERATE]=1;par[P_FMERANGE]=1;par[P_FMEFOLLOW]=1;par[P_FMEPATTERN]=1;
    fm_seq_init();
}
static void advance(int n){while(n>0){fm_seq_fire_due();int k=n>256?256:n;int w=fm_seq_wait_next();if(w>0&&w<k)k=w;assert(k>0);fm_seq_step(k);test_clock+=k;n-=k;}fm_seq_fire_due();}
static int pending(void){int c=0;for(int i=0;i<FM_NWAIT;i++)c+=fm_seq_wait[i].active;return c;}
static void all_notes_off(void){for(int i=0;i<128;i++)assert(!active[i]);}
static int active_count(void){int n=0;for(int i=0;i<128;i++)n+=active[i];return n;}
static void midi_polyphony(void){
    assert(FM_PORT_VOICES==6&&FM_MIDI_VOICES==FM_PORT_VOICES);
    assert(sizeof fm_midi_gate==48u);
    for(int voices=1;voices<=FM_PORT_VOICES;voices++){
        init();par[P_MIDI]=1;par[P_FMPOLY]=(int16_t)voices;
        for(int i=0;i<voices;i++)fm_seq_midi_on(60+i,100,1000u+(uint32_t)i*100u);
        assert(active_count()==voices&&midi_on==voices&&!midi_off);
        fm_seq_midi_on(59+voices,90,2000);
        assert(active_count()==voices&&active[59+voices]==1&&midi_off==1);
        fm_seq_midi_on(90,100,3000);
        assert(active_count()==voices&&!active[60]&&active[90]==1&&midi_off==2);
        advance(3500);all_notes_off();assert(midi_on==midi_off);
    }
    init();par[P_MIDI]=1;par[P_FMPOLY]=6;
    for(int i=0;i<6;i++)fm_seq_midi_on(60+i,100,10000);
    par[P_FMPOLY]=2;fm_seq_midi_on(80,100,10000);
    assert(active_count()==2&&active[61]&&active[80]);
    for(int i=62;i<70;i++)assert(!active[i]);
    par[P_FMPOLY]=6;
    for(int i=81;i<89;i++)fm_seq_midi_on(i,100,10000);
    assert(active_count()==6);
    par[P_MIDI]=0;fm_seq_stop();all_notes_off();assert(midi_on==midi_off);
    init();par[P_MIDI]=1;par[P_FMPOLY]=100;
    for(int i=0;i<12;i++)fm_seq_midi_on(60+i,100,10000);
    assert(active_count()==6);fm_seq_stop();all_notes_off();
    par[P_FMPOLY]=0;
    for(int i=0;i<3;i++)fm_seq_midi_on(72+i,100,10000);
    assert(active_count()==1&&active[74]);fm_seq_stop();all_notes_off();
    puts("MIDI polyphony 1..6, six-slot gates, stealing, retrigger, reduction, expansion and bounds passed");
}
static void fractional_rates(void){
    static const int num[13]={1,2,1,2,1,2,1,3,2,5,3,7,4};
    static const int den[13]={4,7,3,5,2,3,1,2,1,2,1,2,1};
    for(int ext=0;ext<2;ext++)for(int mode=1;mode<=3;mode++)for(int rate=0;rate<13;rate++){
        init();step_len=6000;fm_quarter_samples=24000;en_ext=ext;
        par[P_FMSEQMODE]=mode;par[P_FMERATE]=par[P_FPROTRATE]=par[P_FUDRATE]=seq_rate_from_index(rate);
        par[P_FMLEVEL]=60;par[P_FMEMUTATE]=0;fm_evo_randomize();
        fm_seq_select();capture_ticks=1;tick_count=test_clock=0;
        for(unsigned grid=0;grid<112;grid++){fm_seq_tick(grid);advance(6000);}
        int expected=(28*num[rate]+den[rate]-1)/den[rate];assert(tick_count==expected);
        for(int i=0;i<tick_count;i++){
            double want=(double)i*24000*den[rate]/num[rate];
            double err=tick_time[i]-want;assert(err>=-0.501 && err<=(mode==1?2599.501:0.501));
        }
        fm_rate_wait=100;int before=tick_count;fm_seq_stop();advance(600);assert(tick_count==before);
        capture_ticks=0;
    }
    puts("PASS 13 rates: production FM Hallucigenia/Proteus/UNIT-D rate counts/timing (Hallucigenia retains 0..2599-sample humanization), internal/external clocks, stop cancellation");
}
int main(void){
    fractional_rates();
    init();
    size_t bytes=sizeof fm_evo_core+sizeof fm_seq_rng+sizeof fm_loop_note+sizeof fm_loop_vel+sizeof fm_loop_pos+sizeof fm_loop_rest+sizeof en_fm_loop_pos+sizeof en_fm_loop_note+sizeof en_fm_loop_glow+sizeof fm_evo_bank+sizeof fm_evo_version+sizeof en_fm_evo_active+sizeof en_fm_evo_pattern+sizeof en_fm_evo_pos+sizeof en_fm_evo_trace+sizeof fm_evo_next+sizeof fm_evo_wrapped+sizeof fm_seq_wait+sizeof fm_midi_gate+sizeof fm_tick_age+sizeof fm_quarter_samples+sizeof fm_tick_seen;
    assert(bytes<=1152);printf("Sequencer declared state: %zu bytes\n",bytes);
    en_evo_bank_t saved,got;en_fmevo_snapshot(&saved);fm_evo_randomize();en_fmevo_restore_boot(&saved);en_fmevo_snapshot(&got);assert(!memcmp(&saved,&got,sizeof got));
    saved.magic=0;en_fmevo_restore_boot(&saved);en_fmevo_snapshot(&saved);assert(!memcmp(&saved,&got,sizeof got));
    for(int root=0;root<12;root++)for(int scale=0;scale<3;scale++){
        par[P_ROOT]=root;int maj[]={0,2,4,5,7,9,11},minor[]={0,2,3,5,7,8,10},custom[]={0,1,6};
        nsteps=scale==2?3:7;memcpy(steps,scale==0?maj:scale==1?minor:custom,nsteps*sizeof(int));
        chord_nt=3;for(int i=0;i<3;i++)chord_pcs[i]=(root+steps[(i*2)%nsteps])%12;
        for(int follow=0;follow<2;follow++)for(int oct=-2;oct<=2;oct++)for(int span=1;span<=3;span++)for(int value=0;value<128;value++){
            par[P_FMEFOLLOW]=follow;par[P_FMOCT]=oct;par[P_FMERANGE]=span;int n=fm_evo_pitch((float)value/128.f),pc=(n%12+12)%12,ok=0;
            if(follow){for(int i=0;i<chord_nt;i++)ok|=pc==chord_pcs[i];}else{for(int i=0;i<nsteps;i++)ok|=pc==(root+steps[i])%12;}
            assert(ok);int base=60+root+12*oct;assert(n>=base&&n<base+12*span);
        }
    }
    puts("Harmonic mapping: 138240 cases passed");
    static const int rate_divisors[5]={16,8,4,2,1};
    for(int rate=-1;rate<=3;rate++){
        init();par[P_FMLEVEL]=60;par[P_FMSEQMODE]=1;par[P_FMERATE]=rate;par[P_FMEMUTATE]=0;
        for(int i=0;i<16;i++)fm_evo_bank.step[0][i]=128u+(unsigned)i;
        hall_load(&fm_evo_core,fm_evo_bank.step[0]);
        fm_seq_select();for(unsigned grid=0;grid<96;grid++){fm_seq_tick(grid);advance(5513);assert(triggers==(int)(grid/(unsigned)rate_divisors[rate+1])+1);assert(last_gate==16538);}
        assert(triggers==96/rate_divisors[rate+1]);
    }
    puts("All five rates: exact sixteenth positions and unchanged quarter-note gates passed");
    init();par[P_FMLEVEL]=0;for(unsigned grid=0;grid<16;grid++){fm_seq_tick(grid);advance(5513);}assert(!triggers&&!pending());for(int i=0;i<4;i++)assert(fm_loop_pos[i]==4);
    par[P_FMSEQMODE]=1;fm_seq_tick(17);assert(!en_fm_evo_active);fm_seq_tick(32);assert(en_fm_evo_active);
    par[P_FMEPATTERN]=16;fm_seq_tick(33);assert(en_fm_evo_pattern==0);fm_seq_tick(48);assert(en_fm_evo_pattern==15);
    par[P_FMLEVEL]=60;par[P_MODE]=MODE_PIANO;fm_seq_tick(64);assert(!pending());
    init();for(unsigned grid=0;grid<4*377;grid++){fm_seq_tick(grid);advance(5513);}for(int i=0;i<4;i++)assert(fm_loop_pos[i]==377%EN_LOOP_LEN[i]);
    puts("Original loop lengths, mute, PIANO, bar selection passed");
    init();par[P_FMLEVEL]=60;par[P_MIDI]=1;fm_seq_schedule(60,100,0);fm_seq_fire_due();assert(triggers==1&&midi_on==1&&last_gate==16538);advance(100);fm_seq_schedule(60,90,0);fm_seq_fire_due();assert(midi_on==2&&midi_off==1);advance(16538);all_notes_off();assert(midi_off==2);
    for(int i=0;i<7;i++){fm_seq_schedule(60+i,100,0);fm_seq_fire_due();}int gated=0;for(int i=0;i<128;i++)gated+=active[i];assert(gated==4);fm_seq_stop();all_notes_off();assert(midi_off==midi_on);
    fm_seq_schedule(70,100,0);fm_seq_fire_due();par[P_MIDI]=0;fm_seq_midi_off_all();all_notes_off();
    init();par[P_FMLEVEL]=60;en_ext=1;fm_seq_tick(0);advance(2000);fm_seq_tick(1);fm_seq_schedule(60,100,0);fm_seq_fire_due();assert(last_gate==6000);
    puts("Gate timing, external clock, MIDI stealing/retrigger/disable/stop passed");
    midi_polyphony();
    init();par[P_FMEDENSITY]=0;fm_evo_randomize();for(int i=0;i<16;i++)assert(!(fm_evo_bank.step[0][i]&128));par[P_FMEDENSITY]=100;fm_evo_randomize();for(int i=0;i<16;i++)assert(fm_evo_bank.step[0][i]&128);
    en_fmevo_snapshot(&saved);int8_t notes[4][29];memcpy(notes,fm_loop_note,sizeof notes);fm_seq_schedule(60,100,100);fm_seq_stop();en_fmevo_snapshot(&got);assert(!memcmp(&saved,&got,sizeof got)&&!memcmp(notes,fm_loop_note,sizeof notes)&&!pending());
    puts("Bank restore, density endpoints, stop retention passed");
    puts("FM sequencer checks PASSED");return 0;
}
