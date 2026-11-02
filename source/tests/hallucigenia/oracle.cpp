/* Test the production port against the actual vendored upstream Module class. */
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <cstring>
static uint32_t rng;
static float uniform_value(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return float(rng>>8)*(1.f/16777216.f);}
namespace random { static float uniform(){return uniform_value();} }
namespace dsp {
struct SchmittTrigger { bool high=false; bool process(float v){bool h=v>=1.f,edge=h&&!high;high=h;return edge;} };
struct PulseGenerator { float left=0; void trigger(){left=.001f;} bool process(float dt){bool on=left>0;left-=dt;return on;} };
}
struct Param {float v=0;float getValue(){return v;}void setValue(float n){v=n;}};
struct Input {float v[16]={};float getVoltage(int i=0){return v[i];}bool isConnected(){return false;}};
struct Output {float v=0;void setVoltage(float n){v=n;}};
struct Light {void setBrightness(float){}};
struct ProcessArgs{float sampleTime=1.f/44100.f;};
struct Module {
 Param params[600];Input inputs[16];Output outputs[16];Light lights[16];
 struct {void *producerMessage=nullptr,*consumerMessage=nullptr;} rightExpander;
 virtual void process(const ProcessArgs&){}
 void config(int,int,int,int){}
 void configParam(int i,float,float,float def,const char*){params[i].v=def;}
};
#include "upstream_module.inc"
#include "../../app/dsp/hallucigenia_core.h"
static void same(const Darwinism &u,const hall_state_t &s){for(int i=0;i<16;i++){assert(u.params[Darwinism::NOTE_PARAM+i].v==s.note[i]);assert(u.params[Darwinism::ONOFF_PARAM+i].v==s.on[i]);}}
int main(){
 Darwinism u;hall_state_t s={};ProcessArgs args;uint8_t step=0;
 u.params[Darwinism::SNH_PARAM].v=1;
 for(int cycle=0;cycle<10000;cycle++){
  float density=float(cycle%101)*.01f,rate=float((cycle*17)%101)*.01f;
  u.params[Darwinism::DENSITY_PARAM].v=density;u.params[Darwinism::MUTATIONRATE_PARAM].v=rate;
  u.params[Darwinism::RANDOM_PARAM].v=1;
  rng=uint32_t(cycle+1);u.process(args);uint32_t after=rng;
  rng=uint32_t(cycle+1);hall_randomise(&s,density,uniform_value);assert(rng==after);same(u,s);
  u.params[Darwinism::RANDOM_PARAM].v=0;u.process(args);
  int length=2+cycle%15;u.params[Darwinism::STEPS_PARAM].v=float(length);
  u.step=cycle%16;step=uint8_t(u.step);
  u.inputs[Darwinism::CLOCK_INPUT].v[0]=10;
  u.params[Darwinism::MUTATE_PARAM].v=1;
  rng=uint32_t(cycle+91);u.process(args);after=rng;
  hall_clock(&step,length);assert(step==u.step);
  bool triggered=s.on[step]!=0;float held=s.note[step];
  rng=uint32_t(cycle+91);hall_mutate(&s,rate,density,uniform_value);assert(rng==after);same(u,s);
  if(triggered)assert(u.outputs[Darwinism::NOTE_OUTPUT].v==held);
  u.inputs[Darwinism::CLOCK_INPUT].v[0]=0;u.params[Darwinism::MUTATE_PARAM].v=0;u.process(args);
 }
 uint8_t packed[16],roundtrip[16];
 for(int v=0;v<256;v++){for(int i=0;i<16;i++)packed[i]=uint8_t(v);hall_load(&s,packed);hall_store(&s,roundtrip);assert(!memcmp(packed,roundtrip,16));}
 puts("PASS 10000 upstream comparisons: random notes/density, all-16 mutation, RNG consumption, lengths 2..16, clock-before-output and S&H-before-mutation; all 256 saved byte values round-trip");
}
