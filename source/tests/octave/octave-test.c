#include <assert.h>
#include <stdio.h>
#include "../../app/dsp/amb.c"
static int last_note, last_status, prior_off;
void en_midi_out(uint32_t st,uint32_t n,uint32_t vel) { last_note=n; last_status=st; if ((st&0xf0)==0x80) prior_off=n; }
int main(void) {
 for(int note=24;note<=115;note++) for(int r=0;r<3;r++) for(int c=0;c<=100;c++) {
  for(int k=0;k<100;k++) {
   int v=seq_note(note,r,c,(k+0.5f)/100.0f,(k%2)?0.75f:0.25f);
   assert((v%12+12)%12==note%12);
   int oct=(v-note)/12;
   if(!r||!c) assert(oct==0);
   else if(r==1) assert(oct==0||oct==1);
   else assert(oct>=-1&&oct<=1);
   if(r&&c==100) assert(v!=note);
  }
 }
 for(int c=0;c<=100;c++) {int moved=0;for(int k=0;k<10000;k++) moved+=seq_octave(2,c,(k+0.5f)/10000.0f,0.1f)!=0;assert(moved==100*c);}
 for(int p=0;p<P_NPARAMS;p++) par[p]=en_param_info(p)->def;
 par[P_MIDI]=1; en_playing=1;
 pad_t v={0};v.target=77;
 par[P_PADRANGE]=1;par[P_PADCHANCE]=100;par[P_PADOCT]=1;
 pad_breathe(&v,0); assert(v.emitted_note==77); assert(v.seq_shift==12);
 par[P_PADOCT]=-1;par[P_PADCHANCE]=0;
 pad_breathe(&v,0);assert(prior_off==77);assert(v.emitted_note==41);assert(v.seq_shift==0);
 par[P_PAD2RANGE]=2;par[P_PAD2CHANCE]=100;v.midi_note=0;
 for(int i=0;i<100;i++){pad2_breathe(&v,0);assert(v.seq_shift==-12||v.seq_shift==12);assert(v.emitted_note==41||v.emitted_note==65);}
 nsteps=7; for(int j=0;j<7;j++)steps[j]=j; deg=0; chord_nt=3;chord_pcs[0]=0;chord_pcs[1]=4;chord_pcs[2]=7;
 loop_note[0][0]=18;loop_vel[0][0]=80;par[P_RBURST]=100;
 for(int range=0;range<3;range++)for(int chance=0;chance<=100;chance+=100)for(int base=-2;base<=2;base++)for(int j=0;j<100;j++){
  par[P_RINGRANGE]=range;par[P_RINGCHANCE]=chance;par[P_BOCT]=base;rings_wait_clear();bell_ring(0,0);
  int selected=ring_wait[0].note/12;
  int original=bell_note(0,loop_note[0][0])/12;
  if(!range||!chance)assert(selected==original);
  else if(range==1)assert(selected==original+1);
  else assert(selected==original-1||selected==original+1);
  for(int k=0;k<NRING_WAIT;k++)if(ring_wait[k].active)assert(ring_wait[k].note/12==selected);
 }
 for(int l=0;l<4;l++)for(int rt=0;rt<12;rt++)for(int d=0;d<7;d++)for(int off=0;off<28;off++)for(int bo=-2;bo<=2;bo++) {
  par[P_ROOT]=rt;deg=d;par[P_BOCT]=bo;par[P_RINGRANGE]=0;par[P_RINGCHANCE]=100;par[P_RBURST]=0;
  loop_note[l][0]=off;loop_vel[l][0]=80;
  int expected=bell_note(l,off);rings_wait_clear();bell_ring(l,0);
  assert(ring_wait[0].note==expected);
 }
 puts("PASS: pitch classes, octave bounds, 0/100% endpoints, probability sweep, independent pads and matching MIDI note-off.");
}
