/* SPDX-License-Identifier: GPL-3.0-or-later
 * Isolated UNIT-D production-DSP tests: no copied pseudocode, include source.
 * cc -fsanitize=address,undefined -DPLAT_HOST ... unitd_core_test.c -lm
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../app/dsp/amb.h"
#include "../../base/include/fastmath.h"
#include "../../app/dsp/evolve_rate.h"
#define EN_POOL
#define BARRIER() __asm__ __volatile__("" ::: "memory")
static int16_t par[P_NPARAMS];
static int chord_nt=4,chord_pcs[4]={0,4,7,11};
static float step_len=5512.5f,fm_quarter_samples=22050.f;
volatile uint8_t en_ext,en_playing=1;
static int emits[2],last_note[2],last_vel[2],invalid;
static int note_ok(int note){for(int i=0;i<chord_nt;i++)if(note%12==chord_pcs[i])return 1;return 0;}
static void fm_seq_schedule(int note,int vel,int wait){assert(!wait);assert(note>=0&&note<128);assert(vel>0&&vel<=127);emits[1]++;last_note[1]=note;last_vel[1]=vel;if(!note_ok(note))invalid++;}
static void ring_schedule(int note,int vel,int wait){assert(wait>=0&&wait<8);assert(note>=0&&note<128);assert(vel>0&&vel<=127);emits[0]++;last_note[0]=note;last_vel[0]=vel;if(!note_ok(note))invalid++;}
static void midi(uint32_t st,uint32_t note,uint32_t vel){(void)st;(void)note;(void)vel;}
#include "../../app/dsp/unitd.c"
static void defaults(void){
  memset(par,0,sizeof par);memset(emits,0,sizeof emits);invalid=0;
  for(int l=0;l<2;l++){
    int b=l?P_FUDSEED:P_RUDSEED;
    par[b]=42;par[b+1]=24;par[b+2]=100;par[b+3]=60;
    par[b+4]=1;par[b+5]=0;par[b+6]=45;par[b+7]=75;
    par[b+8]=4;par[b+9]=2;par[b+10]=1;par[b+11]=l?8:4;
  }
  par[P_FMLEVEL]=80;par[P_ROOT]=0;par[P_MODE]=0;
  chord_nt=4;int ch[]={0,4,7,11};memcpy(chord_pcs,ch,sizeof ch);
  ud_init();
}
int main(void){
  defaults();
  assert(sizeof(en_ud_bank_t)==36);
  for(int layer=0;layer<2;layer++){
    ud_reset(layer);
    for(int i=0;i<80;i++)ud_tick(layer);
    assert(emits[layer]>4);
    en_unitd_screen_t v;en_unitd_read(layer,&v);
    assert(v.count==24&&v.poly==(layer?8:4));
    for(int n=0;n<v.count;n++)for(int j=0;j<v.count;j++)if(v.edges[n]&(1ull<<j)){
      assert(n!=j);assert(v.edges[j]&(1ull<<n));
    }
    en_ud_bank_t saved;en_unitd_snapshot(&saved);
    assert(saved.magic==UD_MAGIC&&saved.seq[layer].step>0);
    printf("UNIT-D %s: 80 clocks, %d notes, %u nodes, %u voices\n",layer?"FM":"Rings",emits[layer],v.count,v.poly);
  }
  assert(!invalid);
  /* Rebuild geometries across supported ranges. Seeds must be deterministic. */
  for(int layer=0;layer<2;layer++)for(int nodes=8;nodes<=64;nodes+=8)for(int mode=0;mode<3;mode++){
    int b=ud_base(layer);
    for(int seed=0;seed<=70;seed+=7){
      par[b]=seed;par[b+1]=nodes;par[b+4]=mode;
      par[b+2]=25+seed*2;par[b+3]=30+seed;par[b+8]=1+seed%20;
      ud_reset(layer);ud_tick(layer);
      ud_state_t *s=&ud_state[layer];
      uint32_t hash=0;
      for(int j=0;j<nodes;j++)hash^=(uint32_t)(s->nodes[j].degree+1)*ud_hash((uint32_t)j);
      uint32_t saved[UD_POLY];memcpy(saved,s->rng,sizeof saved);
      ud_reset(layer);ud_tick(layer);
      uint32_t hash2=0;for(int j=0;j<nodes;j++)hash2^=(uint32_t)(s->nodes[j].degree+1)*ud_hash((uint32_t)j);
      assert(hash==hash2&&!memcmp(saved,s->rng,sizeof saved));
      for(int j=0;j<16;j++)ud_tick(layer);
      assert(s->node_count==nodes&&s->poly>0);
    }
  }
  puts("Deterministic graph: 528 parameter combinations, 8..64 nodes and all three WALK modes passed");
  /* 32-step and 16-step lock fully freeze actual captured node+gate patterns. */
  defaults();
  for(int layer=0;layer<2;layer++)for(int sign=-1;sign<=1;sign+=2){
    ud_reset(layer);int b=ud_base(layer);par[b+7]=100;
    for(int j=0;j<64;j++)ud_tick(layer);
    par[b+5]=sign*100;
    uint8_t history[32];int len=sign<0?32:16;
    for(int j=0;j<len*2;j++){
      ud_tick(layer);if(j<len)history[j]=ud_state[layer].v_node[0];
      else assert(ud_state[layer].v_node[0]==history[j%len]);
    }
    assert(ud_state[layer].locked_len==len);
    par[b+5]=0;ud_tick(layer);assert(!ud_state[layer].was_locking);
  }
  puts("LOCK: complete 16-step and 32-step capture loops repeat and unlock safely");
  defaults();
  /* Every root, two chord shapes, octave extremes, and all 5 rates. */
  for(int root=0;root<12;root++)for(int minor=0;minor<2;minor++){
    par[P_ROOT]=root;
    int intervals[4]={0,minor?3:4,7,minor?10:11};
    for(int j=0;j<4;j++)chord_pcs[j]=(root+intervals[j])%12;
    for(int layer=0;layer<2;layer++){
      int b=ud_base(layer);
      for(int oct=-4;oct<=2;oct++){
        par[layer?P_FMOCT:P_BOCT]=oct;
        ud_reset(layer);
        for(int j=0;j<24;j++)ud_tick(layer);
      }
      for(int rate=-1;rate<=3;rate++){
        par[b+10]=rate;ud_reset(layer);
        int ticks=0;
        for(int i=0;i<160;i++)if(i%evolve_rate_divisor(rate)==0){ud_tick(layer);ticks++;}
        assert(ticks==160/(int)evolve_rate_divisor(rate));
      }
    }
  }
  assert(!invalid);puts("Chord-lock: all 12 roots, minor/major, 7 octaves, 5 rates, both engines passed");
  defaults();
  for(int layer=0;layer<2;layer++){
    int b=ud_base(layer);
    par[b+5]=100;par[b+7]=100;
    ud_reset(layer);int initial=emits[layer];
    for(int i=0;i<24;i++)ud_tick(layer);
    assert(emits[layer]>initial);
    assert(ud_state[layer].capture_count==UD_CAPTURE);
    assert(ud_state[layer].locked_len==16);
  }
  puts("Cold-start LOCK: populated non-silent loop for saved locked projects passed");
  defaults();par[P_MODE]=MODE_PIANO;ud_tick(0);ud_tick(1);assert(!emits[0]&&!emits[1]);
  puts("UNIT-D DSP PRODUCTION TESTS PASSED");return 0;
}
