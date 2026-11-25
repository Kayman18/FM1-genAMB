/* SPDX-License-Identifier: ISC
 * genAMB v17.99.8 classic filter bank (original implementation).
 * Independent channel states now serve two stereo Pad buses before reverb sends.
 * No FAUST/Diode/Korg35/Moog algorithm is retained in the active bank.
 *
 * Clean: linear topology-preserving state-variable filter.
 * SEM character: two-pole SVF with a soft input stage, softer resonance law
 * and voltage-range limiting. Same topology family, different nonlinear path.
 * Juno character: four equal TPT poles with an algebraically solved global
 * feedback loop and partial resonance gain compensation.
 * Polysix character: four individually saturated, slightly spread poles,
 * nonlinear feedback solved using three bounded Newton iterations per sample.
 * CS-80 character: independent two-pole HP and LP states in series. With one
 * cutoff control the HP is linked two octaves below the LP, not independent.
 * These are analog-inspired algorithms, not measured circuit replicas.
 */
#include "fm_filter.h"
#include "plat.h"
#include <string.h>

static inline __attribute__((always_inline)) float ff_bound(float x, float lo, float hi) {
    return x < lo ? lo : x > hi ? hi : x;
}
/* Rational tanh approximation with unity small-signal gain, continuous first
 * derivative at the +/-3 endpoints. No transcendentals in the audio loop. */
static PLAT_HOT float ff_sat(float x) {
    if (x >= 3.0f) return 1.0f;
    if (x <= -3.0f) return -1.0f;
    float x2=x*x;
    return x*(27.0f+x2)/(27.0f+9.0f*x2);
}
static PLAT_HOT float ff_sat_derivative(float x) {
    if (x >= 3.0f || x <= -3.0f) return 0.0f;
    float x2=x*x, d=27.0f+9.0f*x2;
    float a=9.0f-x2;
    return 9.0f*a*a/(d*d);
}
static PLAT_HOT float ff_voltage(float x, float limit) {
    return limit*ff_sat(x/limit);
}
static PLAT_HOT float ff_flush(float x) {
    return x < 1.0e-20f && x > -1.0e-20f ? 0.0f : x;
}
/* Low-frequency limit is retained; rational sin/cos expansions are accurate
 * across this cutoff range (20..~17.58k at fixed 44.1kHz). Control-rate only. */
static float ff_cutoff_g_scaled(float normalized, float scale) {
    float e=ff_bound(normalized,0.0f,1.0f)*9.78f;
    int octave=(int)e;
    float f=e-(float)octave;
    float x=0.0014247586f*(1.0f+f*(0.69314718f+f*(0.24022651f+
        f*(0.05550411f+f*(0.00961813f+f*0.00133336f)))));
    while(octave-- > 0) x*=2.0f;
    x*=scale;
    float x2=x*x;
    float sn=x*(1.0f+x2*(-0.16666667f+x2*(0.008333333f+
        x2*(-0.0001984127f+x2*0.000002755732f))));
    float cs=1.0f+x2*(-0.5f+x2*(0.041666667f+
        x2*(-0.0013888889f+x2*0.000024801587f)));
    return sn/cs;
}
static float ff_cutoff_g(float normalized) {
    return ff_cutoff_g_scaled(normalized,1.0f);
}
static inline float ff_svf_scaled(float x,float *z,float g,float damping,int type,float a) {
    float v1=(z[0]+g*(x-z[1]))*a;
    float v2=z[1]+g*v1;
    z[0]=ff_flush(2.0f*v1-z[0]);
    z[1]=ff_flush(2.0f*v2-z[1]);
    float hp=x-damping*v1-v2;
    if(type==1)return hp;
    if(type==2)return damping*v1; /* peak-normalized bandpass */
    if(type==3)return hp+v2;
    return v2;
}
static inline float ff_svf(float x,float *z,float g,float damping,int type) {
    return ff_svf_scaled(x,z,g,damping,type,1.0f/(1.0f+g*(g+damping)));
}
static PLAT_HOT float ff_ladder_linear(fm_filter_history_t *fs,float x,float g,float resonance) {
    float G=g/(1.0f+g), b=1.0f-G;
    float G2=G*G, G3=G2*G, G4=G3*G, k=3.85f*resonance;
    float s=b*(G3*fs->z[0]+G2*fs->z[1]+G*fs->z[2]+fs->z[3]);
    float u=(ff_voltage(x,2.5f)*(1.0f+0.65f*k)-k*s)/(1.0f+k*G4);
    for(int j=0;j<4;j++) {
        float v=(u-fs->z[j])*G;
        u=v+fs->z[j];
        fs->z[j]=ff_flush(u+v);
    }
    return u;
}
/* Evaluate the saturated cascade without changing integrator history; used
 * for the Newton solve, then commit exactly once. */
static PLAT_HOT float ff_poly_eval(fm_filter_history_t *fs,float u,const float *G,int commit,float *derivative) {
    static const float limits[4]={1.65f,1.85f,2.05f,2.25f};
    float d=1.0f;
    for(int j=0;j<4;j++) {
        float t=u/limits[j];
        d*=G[j]*ff_sat_derivative(t);
        float y=G[j]*(limits[j]*ff_sat(t))+(1.0f-G[j])*fs->z[j];
        if(commit) fs->z[j]=ff_flush(2.0f*y-fs->z[j]);
        u=y;
    }
    *derivative=d;
    return u;
}
static PLAT_HOT float ff_ladder_nonlinear(fm_filter_history_t *fs,float x,float g,float resonance) {
    static const float spread[4]={0.96f,1.02f,0.99f,1.04f};
    float G[4],product=1.0f,s=0.0f;
    for(int j=0;j<4;j++) {
        float gj=g*spread[j];G[j]=gj/(1.0f+gj);
        s=G[j]*s+(1.0f-G[j])*fs->z[j];product*=G[j];
    }
    float k=3.80f*resonance;
    float target=ff_voltage(x,2.0f)*(1.0f+0.80f*k);
    float u=(target-k*s)/(1.0f+k*product),d;
    for(int j=0;j<3;j++) {
        float y=ff_poly_eval(fs,u,G,0,&d);
        u-=(u+k*y-target)/(1.0f+k*d);
        u=ff_bound(u,-16.0f,16.0f);
    }
    return ff_poly_eval(fs,u,G,1,&d);
}
static PLAT_HOT float ff_process(const fm_filter_state_t *fs,fm_filter_history_t *ch,float input) {
    float r=fs->resonance,g=fs->g,y;
    switch(fs->model) {
    case FM_FILTER_CLEAN:
        return ff_svf(input,ch->sv,g,1.41421356f-1.32f*r,fs->type);
    case FM_FILTER_SEM:
        y=ff_svf(ff_voltage(input,1.8f),ch->sv,g,1.6f-1.36f*r,fs->type);
        /* Soft voltage rails, deliberately outside the normal quiet range. */
        ch->sv[0]=ff_voltage(ch->sv[0],12.0f);
        ch->sv[1]=ff_voltage(ch->sv[1],12.0f);
        return ff_voltage(y,3.5f);
    case FM_FILTER_JUNO:
        return ff_ladder_linear(ch,input,g,r);
    case FM_FILTER_POLYSIX:
        return ff_ladder_nonlinear(ch,input,g,r);
    case FM_FILTER_CS80: {
        /* Shared cutoff links the HP exactly two octaves below the LP;
         * coefficients are calculated at control rate and smoothed. */
        y=ff_svf(input,ch->high,fs->gh,1.41421356f-0.85f*r,1);
        y=ff_voltage(y,2.2f);
        return ff_svf(y,ch->sv,g,1.41421356f-1.14f*r,0);
    }
    default:return input;
    }
}
void classic_filter_init(fm_filter_state_t *fs) {
    memset(fs,0,sizeof *fs);
    fs->g=fs->target_g=ff_cutoff_g(0.78f);
    fs->gh=fs->target_gh=ff_cutoff_g_scaled(0.78f,0.25f);
    fs->resonance=fs->target_res=0.25f;
    fs->wet=fs->target_wet=1.0f;
}
void classic_filter_set(fm_filter_state_t *fs,int model,int type,int cutoff,int resonance,int wet,
                   float mod_cutoff,float mod_res) {
    model=model<0?0:model>=FM_FILTER_COUNT?FM_FILTER_COUNT-1:model;
    type=type<0?0:type>3?3:type;
    if(model==FM_FILTER_BYPASS || model>=FM_FILTER_JUNO)type=0;
    fs->pending_model=(uint8_t)model;fs->pending_type=(uint8_t)type;
    fs->target_g=ff_cutoff_g((float)cutoff*0.01f+mod_cutoff*0.20f);
    fs->target_gh=ff_cutoff_g_scaled((float)cutoff*0.01f+mod_cutoff*0.20f,0.25f);
    fs->target_res=ff_bound((float)resonance*0.01f+mod_res*0.65f,0.0f,0.97f);
    fs->target_wet=ff_bound((float)wet*0.01f,0.0f,1.0f);
}
static inline void ff_advance(fm_filter_state_t *fs,fm_filter_history_t *right) {
    /* Fade the filtered contribution through dry before a topology switch;
     * never reset a full-wet state abruptly. 128 samples each side (~2.9ms). */
    int changing=fs->pending_model!=fs->model || fs->pending_type!=fs->type;
    fs->target_blend=(changing || fs->model==FM_FILTER_BYPASS)?0.0f:1.0f;
    if(fs->blend<fs->target_blend) {
        fs->blend+=0.0078125f;if(fs->blend>1.0f)fs->blend=1.0f;
    } else if(fs->blend>fs->target_blend) {
        fs->blend-=0.0078125f;if(fs->blend<0.0f)fs->blend=0.0f;
    }
    if(changing && fs->blend==0.0f) {
        memset(&fs->history,0,sizeof fs->history);
        if (right) memset(right,0,sizeof *right);
        fs->model=fs->pending_model;fs->type=fs->pending_type;
    }
    fs->g+=(fs->target_g-fs->g)*0.0025f;
    fs->gh+=(fs->target_gh-fs->gh)*0.0025f;
    fs->resonance+=(fs->target_res-fs->resonance)*0.0025f;
    fs->wet+=(fs->target_wet-fs->wet)*0.004f;
    if(fs->target_wet==0.0f && fs->wet<1.0e-6f)fs->wet=0.0f;
}

static inline float ff_mix(float input,float filtered,float mix) {
    float y=ff_bound(filtered,-8.0f,8.0f);
    return mix==0.0f?input:input+(y-input)*mix;
}

PLAT_HOT float classic_filter_sample(fm_filter_state_t *fs,float input) {
    ff_advance(fs,0);
    if(fs->model==FM_FILTER_BYPASS && fs->blend==0.0f)return input;
    return ff_mix(input,ff_process(fs,&fs->history,input),fs->wet*fs->blend);
}

/* Both channels see the same knob, modulation and topology transitions.
 * Only the integrator history can differ between them. */
/* genAMB: one call per block instead of one per sample.
 *
 * Two things were being paid for on every sample. The four control smoothers
 * and the blend ramp were loaded from the struct, updated and stored back; and
 * when the bank is fully dry the filter still ran, with ff_mix() then throwing
 * the result away, because mix = wet * blend is zero and ff_mix() returns the
 * dry input. A dry bank is the normal state of both Pad layers unless the
 * player has dialled a filter mix in, and it was costing a fifth of the whole
 * engine.
 *
 * So: if the bank is dry for certain over this whole block - wet and
 * target_wet both exactly zero, the blend ramp parked, and no topology change
 * pending - do nothing. mix is zero for every sample, so the output is the dry
 * input it already holds. The histories and the smoothed coefficients stop
 * advancing, which is the one behavioural difference: on the first block that
 * is not dry the coefficients are snapped to their targets. Tracking nothing
 * for a few hundred samples is what put them there in the old code too, so in
 * practice the filter opens on the same cutoff it would have.
 *
 * Otherwise the block runs with the five smoothed scalars live in a local the
 * compiler keeps in registers. Every test and every operation keeps its
 * original order, so an active bank is bit-exact. */
typedef struct {
    float g,gh,res,wet,blend,tg,tgh,tr,tw,tb;
    int changing,model,type;
} cf_live;

static inline __attribute__((always_inline)) void cf_load(fm_filter_state_t *fs,cf_live *L) {
    L->g=fs->g;L->gh=fs->gh;L->res=fs->resonance;L->wet=fs->wet;L->blend=fs->blend;
    L->tg=fs->target_g;L->tgh=fs->target_gh;L->tr=fs->target_res;L->tw=fs->target_wet;
    L->model=fs->model;L->type=fs->type;
    L->changing=fs->pending_model!=fs->model || fs->pending_type!=fs->type;
    L->tb=(L->changing || L->model==FM_FILTER_BYPASS)?0.0f:1.0f;
}
static inline __attribute__((always_inline)) void cf_store(fm_filter_state_t *fs,cf_live *L) {
    fs->g=L->g;fs->gh=L->gh;fs->resonance=L->res;fs->wet=L->wet;fs->blend=L->blend;
    fs->target_blend=L->tb;
    fs->model=(uint8_t)L->model;fs->type=(uint8_t)L->type;
}
/* ff_advance(), with the state in the local. */
static inline __attribute__((always_inline)) void cf_advance(fm_filter_state_t *fs,
        fm_filter_history_t *right,cf_live *L) {
    if(L->blend<L->tb) {
        L->blend+=0.0078125f;if(L->blend>1.0f)L->blend=1.0f;
    } else if(L->blend>L->tb) {
        L->blend-=0.0078125f;if(L->blend<0.0f)L->blend=0.0f;
    }
    if(L->changing && L->blend==0.0f) {
        memset(&fs->history,0,sizeof fs->history);
        if (right) memset(right,0,sizeof *right);
        L->model=fs->pending_model;L->type=fs->pending_type;
        L->changing=0;
        L->tb=(L->model==FM_FILTER_BYPASS)?0.0f:1.0f;
    }
    L->g+=(L->tg-L->g)*0.0025f;
    L->gh+=(L->tgh-L->gh)*0.0025f;
    L->res+=(L->tr-L->res)*0.0025f;
    L->wet+=(L->tw-L->wet)*0.004f;
    if(L->tw==0.0f && L->wet<1.0e-6f)L->wet=0.0f;
}

PLAT_HOT void classic_filter_stereo_block(fm_filter_state_t *fs,fm_filter_history_t *right,
                                          float *left,float *right_buf,uint32_t n) {
    cf_live L;
    cf_load(fs,&L);
    if(!L.changing && L.wet==0.0f && L.tw==0.0f && L.blend==L.tb) {
        /* Dry for the whole block: mix is zero on every sample and ff_mix()
         * hands back the dry input, so there is nothing to compute. */
        fs->dormant=1;
        cf_store(fs,&L);
        return;
    }
    if(fs->dormant) {
        L.g=L.tg;L.gh=L.tgh;L.res=L.tr;
        fs->dormant=0;
    }
    for(uint32_t i=0;i<n;i++) {
        cf_advance(fs,right,&L);
        if(L.model==FM_FILTER_BYPASS && L.blend==0.0f) continue;
        float l=left[i], r=right_buf[i];
        float yl,yr;
        if(L.model==FM_FILTER_CLEAN) {
            float g=L.g, d=1.41421356f-1.32f*L.res;
            float a=1.0f/(1.0f+g*(g+d));
            yl=ff_svf_scaled(l,fs->history.sv,g,d,L.type,a);
            yr=ff_svf_scaled(r,right->sv,g,d,L.type,a);
        } else {
            /* ff_process() reads the smoothed scalars from the struct, so keep
             * them there for the models that are not the common one. */
            fs->g=L.g;fs->gh=L.gh;fs->resonance=L.res;
            fs->model=(uint8_t)L.model;fs->type=(uint8_t)L.type;
            yl=ff_process(fs,&fs->history,l);
            yr=ff_process(fs,right,r);
        }
        float mix=L.wet*L.blend;
        left[i]=ff_mix(l,yl,mix);
        right_buf[i]=ff_mix(r,yr,mix);
    }
    cf_store(fs,&L);
}

PLAT_HOT void classic_filter_stereo_sample(fm_filter_state_t *fs,fm_filter_history_t *right,
                                           float *left_sample,float *right_sample) {
    ff_advance(fs,right);
    if(fs->model==FM_FILTER_BYPASS && fs->blend==0.0f)return;
    float l=*left_sample, r=*right_sample;
    float yl,yr;
    if(fs->model==FM_FILTER_CLEAN) {
        float g=fs->g, d=1.41421356f-1.32f*fs->resonance;
        float a=1.0f/(1.0f+g*(g+d));
        yl=ff_svf_scaled(l,fs->history.sv,g,d,fs->type,a);
        yr=ff_svf_scaled(r,right->sv,g,d,fs->type,a);
    } else {
        yl=ff_process(fs,&fs->history,l);
        yr=ff_process(fs,right,r);
    }
    float mix=fs->wet*fs->blend;
    *left_sample=ff_mix(l,yl,mix);
    *right_sample=ff_mix(r,yr,mix);
}

/* Historical mono entry points remain for reference tests. Production FM no
 * longer calls them; the two Pad buses own their independent stereo states. */
static fm_filter_state_t legacy_fm_state;
void fm_filter_init(void){classic_filter_init(&legacy_fm_state);}
void fm_filter_set(int m,int t,int c,int r,int w,float mc,float mr){classic_filter_set(&legacy_fm_state,m,t,c,r,w,mc,mr);}
float fm_filter_sample(float x){return classic_filter_sample(&legacy_fm_state,x);}
