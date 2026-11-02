from pathlib import Path
root=Path(__file__).parent

def edit(path,fn):
 p=root/path;s=p.read_text();res=fn(s);p.write_text(res)
def rep(s,a,b):
 assert a in s, a[:100]
 return s.replace(a,b,1)

def header(s):
 return rep(s,'    P_FPROTGATE, P_FPROTOCT, P_FPROTMUTATE, P_FPROTLOCK, P_FPROTRANGE,\n    P_NPARAMS', '''    P_FPROTGATE, P_FPROTOCT, P_FPROTMUTATE, P_FPROTLOCK, P_FPROTRANGE,
    /* v20, append only: 10 FM voices, post-FM modelled filter and 11 mod sources */
    P_FFILTERMODEL, P_FFILTERTYPE, P_FFILTERCUT, P_FFILTERRES, P_FFILTERMIX,
    P_M7SHAPE, P_M8SHAPE, P_M9SHAPE,
    P_M10DEPTH, P_M10SPEED, P_M10SHAPE,
    P_M11DEPTH, P_M11SPEED, P_M11SHAPE,
    P_NPARAMS''').replace('en_mod_trace[9][EN_MOD_TRACE]','en_mod_trace[11][EN_MOD_TRACE]')
edit(Path('app/dsp/amb.h'),header)

def parameters(s):
 s=rep(s,'    {"Gate",5,100,50}, {"Oct Prob",0,100,20}, {"Mutate",0,100,20}, {"Lock",0,2,0}, {"Range",0,2,1},\n};','''    {"Gate",5,100,50}, {"Oct Prob",0,100,20}, {"Mutate",0,100,20}, {"Lock",0,2,0}, {"Range",0,2,1},
    /* Model 0 is Standard; Type 3=Bypass keeps old v19 projects byte identical. */
    {"Filter model",0,6,0}, {"Filter type",0,3,3}, {"Cutoff",0,100,70},
    {"Resonance",0,100,25}, {"Filter mix",0,100,100},
    {"M7 shape",0,5,0}, {"M8 shape",0,5,0}, {"M9 shape",0,5,0},
    {"M10 depth",-100,100,65}, {"M10 seconds",0,180,90}, {"M10 shape",0,5,2},
    {"M11 depth",-100,100,40}, {"M11 seconds",0,180,135}, {"M11 shape",0,5,4},
};''')
 s=rep(s,'    else if (i == P_FMPATCH) s = fm_port_patch_name(v);','''    else if (i == P_FMPATCH) s = fm_port_patch_name(v);
    else if (i == P_FFILTERMODEL) {
        static const char *const names[]={"Standard","Diode24","Korg35 HP","Korg35 LP","Moog24","Moog12","Oberheim"};
        s=names[(unsigned)v<7u?v:0];
    } else if (i == P_FFILTERTYPE) {
        static const char *const names[]={"Low Pass","High Pass","Band Pass","Bypass"};
        static const char *const ob[]={"Low Pass","High Pass","Band Pass","Notch"};
        int model=par[P_FFILTERMODEL];
        s=(model==6?ob:names)[(unsigned)v<4u?v:0];
        if(model==1||model==3||model==4||model==5)s="Low Pass";
        if(model==2)s="High Pass";
    } else if (i==P_M7SHAPE || i==P_M8SHAPE || i==P_M9SHAPE || i==P_M10SHAPE || i==P_M11SHAPE) {
        static const char *const nm[]={"Smooth Rnd","Step Rnd","Sine","Pulse","Triangle","Saw"};
        s=nm[(unsigned)v<6u?v:0];
    }''')
 s=rep(s,'    if (i == P_M1SPEED || i == P_M2SPEED', '''    if (i == P_M10SPEED || i == P_M11SPEED) {
        b=itoa_s(v,b); *b++='s'; *b=0;return;
    }
    if (i == P_FFILTERCUT) {
        /* Percent display; the filter graph/engine uses a logarithmic mapping. */
        b=itoa_s(v,b);*b++='%';*b=0;return;
    }
    if (i == P_M1SPEED || i == P_M2SPEED''')
 s=rep(s,'#define NMOD 9 ', '#define NMOD 11 ')
 s=s.replace('en_mod_trace[9][EN_MOD_TRACE]','en_mod_trace[11][EN_MOD_TRACE]')
 s=rep(s,'    return i < 6 ? P[i] : i == 6 ? P_M7DEPTH : i == 7 ? P_M8DEPTH : P_M9DEPTH;', '    return i < 6 ? P[i] : i == 6 ? P_M7DEPTH : i == 7 ? P_M8DEPTH : i==8 ? P_M9DEPTH : i==9 ? P_M10DEPTH : P_M11DEPTH;')
 s=rep(s,'    return i < 6 ? P[i] : i == 6 ? P_M7SPEED : i == 7 ? P_M8SPEED : P_M9SPEED;', '    return i < 6 ? P[i] : i == 6 ? P_M7SPEED : i == 7 ? P_M8SPEED : i==8 ? P_M9SPEED : i==9 ? P_M10SPEED : P_M11SPEED;')
 start=s.index('static void mods_step(uint32_t n)')
 end=s.index('static void pad2_mods_step(uint32_t n)',start)
 body='''/* M7..M11's selectable waveforms; 0 preserves M7..M9's legacy
 * independent smooth-random targets, 1 is sample-and-hold stepped random.
 * Sine/triangle/pulse/saw are block-rate control LFOs, not audio oscillators. */
static float mod_phase[5];
static int mod_shape_param(int i) {
    static const int sp[5]={P_M7SHAPE,P_M8SHAPE,P_M9SHAPE,P_M10SHAPE,P_M11SHAPE};
    return sp[i-6];
}
static float wave_shape(float p,int sh) {
    if(sh==2) return sine(p); /* same 1024-point table as the existing engine */
    if(sh==3) return p<0.25f?1.0f:-1.0f;
    if(sh==4) return p<0.5f ? 4.0f*p-1.0f : 3.0f-4.0f*p;
    if(sh==5) return 2.0f*p-1.0f;
    return 0.0f;
}
static void mods_step(uint32_t n)
{
    int i;
    for(i=0;i<NMOD;++i) {
        int shape=i>=6?par[mod_shape_param(i)]:0;
        if(i>=9 && par[mod_speed_param(i)]==0) {mod[i].v=0.0f;continue;}
        if(i>=6 && shape>=2) {
            float sec=i>=9?(float)par[mod_speed_param(i)]:mod_secs_from_speed(par[mod_speed_param(i)]);
            if(sec<0.001f)sec=0.001f;
            float phase=mod_phase[i-6]+(float)n/(sec*EN_SR);
            /* Handle wraps without signed overflow or 64-bit arithmetic. */
            if(phase>=1.0f)phase-=1.0f;
            mod_phase[i-6]=phase;
            mod[i].v=wave_shape(phase,shape);
            continue;
        }
        if(mod[i].left<=n) {
            float secs;
            if(i>=9) secs=(float)par[mod_speed_param(i)];
            else secs=mod_secs_from_speed(par[mod_speed_param(i)]);
            if (i<9)secs*=0.75f+0.5f*frand();
            if(secs<0.001f)secs=0.001f;
            mod[i].tgt=frand()*2.0f-1.0f;
            mod[i].left=(uint32_t)(secs*EN_SR);
            mod[i].rate=(float)n/(secs*EN_SR*0.55f);
        } else mod[i].left-=n;
        if(shape==1)mod[i].v=mod[i].tgt;
        else mod[i].v+=(mod[i].tgt-mod[i].v)*fm_minf(mod[i].rate,1.0f);
    }
    mod_trace_samples+=n;
    if(mod_trace_samples>=EN_SR) {
        mod_trace_samples-=EN_SR;
        for(i=0;i<NMOD;++i)
            en_mod_trace[i][en_mod_trace_head]=(int8_t)(fm_clampf(mod[i].v,-1.0f,1.0f)*127.0f);
        en_mod_trace_head=(en_mod_trace_head+1)%EN_MOD_TRACE;
        if(en_mod_trace_count<EN_MOD_TRACE)en_mod_trace_count++;
    }
}

'''
 s=s[:start]+body+s[end:]
 # Add per-control fm filter update after mods and prior to applying ring ports.
 s=rep(s,'    pad2_mods_step(n);\n    pad2_control_clock(n);','''    pad2_mods_step(n);
    pad2_control_clock(n);
    fm_filter_set(par[P_FFILTERMODEL],par[P_FFILTERTYPE],par[P_FFILTERCUT],
                  par[P_FFILTERRES],par[P_FFILTERMIX],
                  mod[9].v*(float)par[P_M10DEPTH]*0.01f,
                  mod[10].v*(float)par[P_M11DEPTH]*0.01f);''')
 # ensure init sets filter init (already via fm_port_init) and mod_phase zeros in en_init
 s=rep(s,'    mod_trace_samples = 0;', '    mod_trace_samples = 0;\n    memset(mod_phase,0,sizeof(mod_phase));')
 return s
edit(Path('app/dsp/amb.c'),parameters)

def proj_app(s):return s.replace('#define PROJ_FORMAT 19u','#define PROJ_FORMAT 20u').replace('/* 19: Proteus per-engine patterns and settings.\n                                          * Formats 18 and earlier migrate forward. */', '/* 20: FM filter and modulation shapes.\n                                          * Formats 19 and earlier migrate forward. */')
edit(Path('app/app.h'),proj_app)

def project(s):
 old='/* v17.99.3 format 18 is a strict binary prefix before the Proteus parameters. */'
 new='''/* v17.99.4 format 19 has the complete Proteus banks but no filter/shape params. */
#define V19_NPARAMS P_FFILTERMODEL
/* Keep the historical v19 struct byte-exact for migration from the prior release. */
typedef struct {int16_t par[V19_NPARAMS];uint16_t pool;} slot_v19_t;
typedef struct {
    uint32_t magic,format;int16_t par[V19_NPARAMS];uint16_t pool;
    uint8_t leds,piece,rsv0[2];slot_v19_t slot[NPIECES];uint8_t rsv[16];
    en_evo_bank_t evo,fm_evo;en_proteus_bank_t proteus;
} project_v19_t;
static void from_v19(const project_v19_t *o) {
    slot_t r;int i,p;
    memset(&proj,0,sizeof proj);proj.magic=PROJ_MAGIC;proj.format=PROJ_FORMAT;
    proj.pool=o->pool;proj.leds=o->leds;proj.piece=o->piece<NPIECES?o->piece:0;
    for(i=0;i<NPIECES;i++) {
        recipe(i,&r);
        for(p=0;p<P_NPARAMS;p++)proj.slot[i].par[p]=p<V19_NPARAMS?o->slot[i].par[p]:r.par[p];
        proj.slot[i].pool=o->slot[i].pool;
    }
    recipe(proj.piece,&r);
    for(p=0;p<P_NPARAMS;p++)proj.par[p]=p<V19_NPARAMS?o->par[p]:r.par[p];
    proj.evo=o->evo;proj.fm_evo=o->fm_evo;proj.proteus=o->proteus;
}

'''+old
 s=rep(s,old,new)
 s=rep(s,'        project_t now;\n        project_v18_t v18;', '        project_t now;\n        project_v19_t v19;\n        project_v18_t v18;')
 s=rep(s,'    if (n >= (int)sizeof(project_v18_t)', '    if (n >= (int)sizeof(project_v19_t) && buf.now.magic == PROJ_MAGIC && buf.now.format == 19u)\n        from_v19(&buf.v19);\n    else if (n >= (int)sizeof(project_v18_t)')
 return s
edit(Path('app/project.c'),project)
