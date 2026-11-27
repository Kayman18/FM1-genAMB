/* SPDX-License-Identifier: MPL-2.0
 * genAMB Nature VA layer. The existing keyboard, sequencer and worker
 * entry points retain their names for compatibility with the other layers. */
#include "fm_port.h"
#include "amb.h"
#include "fastmath.h"
#include "fm_filter.c"
#include <string.h>

enum { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
typedef struct { float value, release_start; unsigned stage; } envelope_t;
typedef struct { float attack, decay, sustain, release; } adsr_t;
typedef struct {
    envelope_t amp, filter;
    fm_filter_history_t poles;
    float phase, subphase, increment, inverse_increment, velocity, last, residual;
    uint32_t gate, age;
    int note;
} va_voice_t;

static va_voice_t voices[FM_PORT_VOICES];
static adsr_t amp_env, filter_env;
static float blend, sub, pwm, lfo_phase, lfo_step, lfo_cut;
static float cutoff, resonance, env_amount, key_track, gain, target_gain;
static int limit, tuning;
static uint32_t serial;
volatile float fm_port_level;
volatile uint8_t fm_port_active_voices;

static float time_step(float knob)
{
    return 1.0f/(44.1f*(2.0f+0.8f*knob*knob));
}
static adsr_t settings(float a,float d,float s,float r)
{
    adsr_t e={time_step(a),time_step(d),s*0.01f,time_step(r)};
    return e;
}
void va_controls(const int16_t *p,const volatile float *targets)
{
    blend=p[P_VABLEND]*0.01f;
    sub=p[P_VASUB]*0.01f;
    pwm=targets[5]*0.0044f;
    lfo_step=1.0f/(EN_SR*fm_maxf(1,p[P_VALFORATE]));
    lfo_cut=p[P_VALFOCUT]*0.004f;
    cutoff=targets[1]*0.01f;
    resonance=targets[2]*0.01f;
    env_amount=targets[3]*0.006f;
    key_track=targets[4]*(0.01f/(12*9.78f));
    amp_env=settings(p[P_VAAMPA],p[P_VAAMPD],p[P_VAAMPS],p[P_VAAMPR]);
    filter_env=settings(targets[6],targets[7],targets[8],targets[9]);
}
/* A stage normally lasts many slices. Run one loop per stage and only
 * write values for the amp envelope. The VCF needs its endpoint. */
static unsigned envelope_block(envelope_t *e,const adsr_t *settings,float *out,unsigned n)
{
    unsigned i=0;
    while(i<n){
        float x=e->value;
        float delta, target;
        unsigned next;
        switch(e->stage){
        case ATTACK:
            delta=settings->attack;target=1;next=DECAY;break;
        case DECAY:
            delta=-settings->decay*(1-settings->sustain);
            target=settings->sustain;next=SUSTAIN;break;
        case RELEASE:
            delta=-settings->release*e->release_start;target=0;next=IDLE;break;
        default:
            e->value=e->stage==SUSTAIN?settings->sustain:0;
            if(e->stage==IDLE)return i;
            if(out)while(i<n)out[i++]=e->value;
            return n;
        }
        unsigned left=n-i;
        /* A conservative endpoint check keeps stage transitions sample-exact. */
        float end=x+delta*(float)left;
        if((next==DECAY && end<target-0.00001f) ||
           (next!=DECAY && end>target+0.00001f)){
            if(out)while(i<n){x+=delta;out[i++]=x;}
            else for(;i<n;i++)x+=delta;
        }else{
            do{
                x+=delta;
                if(next==DECAY?x>=target:x<=target){x=target;e->stage=next;}
                if(out)out[i]=x;
                i++;
            }while(i<n && e->stage!=next);
        }
        e->value=x;
    }
    return n;
}
static void release_voice(va_voice_t *v)
{
    if(v->amp.stage==IDLE || v->amp.stage==RELEASE)return;
    v->gate=0;
    v->amp.release_start=v->amp.value;
    v->filter.release_start=v->filter.value;
    v->amp.stage=v->filter.stage=RELEASE;
}
void fm_port_panic(void)
{
    memset(voices,0,sizeof voices);
    fm_port_active_voices=0;
    fm_port_level=0;
}
void fm_port_init(void)
{
    fm_port_panic();
    serial=0;
    limit=FM_PORT_DEFAULT_VOICES;
    tuning=0;
    lfo_phase=gain=target_gain=0;
}
void fm_port_note_off(int note)
{
    for(int i=0;i<FM_PORT_VOICES;i++)if(voices[i].note==note)release_voice(&voices[i]);
}
void fm_port_all_notes_off(void)
{
    for(int i=0;i<FM_PORT_VOICES;i++)release_voice(&voices[i]);
}
void fm_port_set_polyphony(int count)
{
    limit=count<1?1:count>FM_PORT_VOICES?FM_PORT_VOICES:count;
    for(int i=limit;i<FM_PORT_VOICES;i++)release_voice(&voices[i]);
}
float fm_port_set_level_modulated(int base,float offset)
{
    float level=fm_clampf(base+offset,0,100);
    /* +6 dB VA bus trim, applied to both the dry signal and its reverb send. */
    target_gain=0.0002f*level*level;
    return level;
}
void fm_port_set_level(int level){fm_port_set_level_modulated(level,0);}
static void set_pitch(va_voice_t *v)
{
    v->increment=440.0f/EN_SR*fm_exp2f(((v->note-69)+0.01f*tuning)/12.0f);
    v->inverse_increment=1.0f/v->increment;
}
void fm_port_set_tune(int cents)
{
    tuning=cents;
    for(int i=0;i<FM_PORT_VOICES;i++)set_pitch(&voices[i]);
}
void fm_port_trigger(int note,int velocity,uint32_t gate)
{
    int slot=0;
    for(int i=0;i<limit;i++){
        va_voice_t *v=&voices[i], *best=&voices[slot];
        if(v->amp.stage==IDLE){slot=i;break;}
        if(v->amp.stage==RELEASE && best->amp.stage!=RELEASE)slot=i;
        else if((v->amp.stage==RELEASE)==(best->amp.stage==RELEASE) &&
                serial-v->age>serial-best->age)slot=i;
    }
    va_voice_t *v=&voices[slot];
    float residual=v->last;
    memset(v,0,sizeof *v);
    v->residual=residual;
    v->note=note;
    v->velocity=fm_clampf(velocity/127.0f,0,1);
    set_pitch(v);
    v->gate=gate;
    v->age=++serial;
    v->amp.stage=v->filter.stage=ATTACK;
}
/* Polynomial band-limited step corrections at the saw and pulse edges. */
static inline float blep(float phase,float step,float inverse)
{
    if(phase<step){float t=phase*inverse-1;return -t*t;}
    if(phase>1-step){float t=(phase-1)*inverse+1;return t*t;}
    return 0;
}
/* The cutoff envelope and LFO are sampled once per 16 frames. Compute
 * the four-pole feedback coefficients on that same boundary. */
typedef struct {
    float pole, complement, square, cube, feedback, makeup, inverse_divisor;
} va_ladder_t;

static inline va_ladder_t ladder_coefficients(float g,float resonance_value)
{
    va_ladder_t c;
    c.pole=g/(1.0f+g);
    c.complement=1.0f-c.pole;
    c.square=c.pole*c.pole;
    c.cube=c.square*c.pole;
    c.feedback=3.85f*resonance_value;
    c.makeup=1.0f+0.65f*c.feedback;
    c.inverse_divisor=1.0f/(1.0f+c.feedback*(c.cube*c.pole));
    return c;
}

/* Keep the four integrator histories in registers across the whole slice.
 * The filter loop has no voice allocation, gate or envelope work. */
static void ladder_block(float *history,const va_ladder_t *c,float *audio,unsigned n)
{
    float z0=history[0],z1=history[1],z2=history[2],z3=history[3];
    for(unsigned i=0;i<n;i++){
        float feedback=c->complement*(c->cube*z0+c->square*z1+c->pole*z2+z3);
        float u=(ff_voltage(audio[i],2.5f)*c->makeup-c->feedback*feedback)*c->inverse_divisor;
        float delta=(u-z0)*c->pole;
        u=z0+delta; z0=u+delta;
        delta=(u-z1)*c->pole;
        u=z1+delta; z1=u+delta;
        delta=(u-z2)*c->pole;
        u=z2+delta; z2=u+delta;
        delta=(u-z3)*c->pole;
        u=z3+delta; z3=u+delta;
        audio[i]=u;
    }
    history[0]=ff_flush(z0);history[1]=ff_flush(z1);history[2]=ff_flush(z2);history[3]=ff_flush(z3);
}

void fm_port_render_add(float *left,float *right,float *send,float amount,uint32_t n)
{
    float meter=fm_port_level;
    int sounding=0;
    for(int i=0;i<FM_PORT_VOICES;i++)sounding|=voices[i].amp.stage;
    if(!sounding){
        fm_port_active_voices=0;
        lfo_phase+=n*lfo_step;
        lfo_phase-=fm_floorf(lfo_phase);
        gain=target_gain;
        fm_port_level=meter<1e-7f?0:meter*fm_exp2f(-0.00072153f*n);
        return;
    }
    for(uint32_t pos=0;pos<n;){
        unsigned count=n-pos<16?n-pos:16;
        float sum[16]={0};
        float tri=1-4*fm_fabsf(lfo_phase-0.5f);
        lfo_phase+=count*lfo_step;
        if(lfo_phase>=1)lfo_phase-=1;
        unsigned active=0;
        for(int j=0;j<FM_PORT_VOICES;j++){
            va_voice_t *v=&voices[j];
            if(v->amp.stage==IDLE)continue;
            float dt=v->increment, inv=v->inverse_increment;
            float width=fm_clampf(0.5f+pwm*tri,dt,1-dt);
            float g=ff_cutoff_g(cutoff+env_amount*v->filter.value+
                                (v->note-60)*key_track+lfo_cut*tri);
            const va_ladder_t coeff=ladder_coefficients(g,resonance);
            float audio[16];
            for(unsigned k=0;k<count;k++){
                float p=v->phase, edge=blep(p,dt,inv), shifted=p-width;
                if(shifted<0)shifted+=1;
                float saw=2*p-1-edge;
                float pulse=(p<width?1.0f:-1.0f)+edge-blep(shifted,dt,inv)-(2*width-1);
                audio[k]=saw+blend*(pulse-saw)+sub*(1-4*fm_fabsf(v->subphase-0.5f));
                v->phase+=dt;
                if(v->phase>=1)v->phase-=1;
                v->subphase+=0.5f*dt;
                if(v->subphase>=1)v->subphase-=1;
            }
            ladder_block(v->poles.z,&coeff,audio,count);
            float amplitude[16];
            unsigned held=(v->gate && v->gate<count)?v->gate:count;
            envelope_block(&v->filter,&filter_env,0,held);
            unsigned live=envelope_block(&v->amp,&amp_env,amplitude,held);
            if(v->gate){
                v->gate-=held;
                if(!v->gate)release_voice(v);
            }
            if(held<count){
                envelope_block(&v->filter,&filter_env,0,count-held);
                live+=envelope_block(&v->amp,&amp_env,amplitude+held,count-held);
            }
            if(v->residual!=0){
                for(unsigned k=0;k<live;k++){
                    v->last=0.23f*audio[k]*amplitude[k]*v->velocity+v->residual;
                    sum[k]+=v->last;
                    v->residual*=0.975f;
                }
            }else{
                for(unsigned k=0;k<live;k++){
                    v->last=0.23f*audio[k]*amplitude[k]*v->velocity;
                    sum[k]+=v->last;
                }
            }
            if(v->amp.stage==IDLE)v->last=0;
            /* Voice stealing leaves a very short de-click tail. Flush it
             * before subnormal arithmetic can persist for a held note. */
            if(fm_fabsf(v->residual)<1.0e-20f)v->residual=0;
            active+=v->amp.stage!=IDLE;
        }
        fm_port_active_voices=(uint8_t)active;
        for(unsigned k=0;k<count;k++){
            gain+=(target_gain-gain)*0.004f;
            float x=sum[k]*gain;
            left[pos+k]+=x;right[pos+k]+=x;
            if(send)send[pos+k]+=x*amount;
            meter=fm_maxf(0.9995f*meter,fm_fabsf(x));
        }
        pos+=count;
    }
    fm_port_level=meter;
}
