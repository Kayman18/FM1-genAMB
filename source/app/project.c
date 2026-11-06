/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 project: one flash object (OBJ_PROJ). A project of another format, or none, gives the
 * defaults. Part of the unity build, after app.h.
 *
 * The pieces (their recipes are the engine's, en_piece): each keeps its own settings as you leave it;
 * Tune and MIDI out are global. */
#include "fm_port/fm_port.h"
project_t proj;
typedef char project_fits_flash_object[(sizeof(project_t) <= PLAT_STORE_MAX) ? 1 : -1];

const char *piece_name(int i) { return en_piece_name(i); }

static int global_par(int p) { return p == P_TUNE || p == P_MIDI; }

static void recipe(int i, slot_t *s) { en_piece(i, s->par, &s->pool); }

static void take(const slot_t *s)                /* a piece's settings into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
    proj.pool = s->pool;
}

static void keep(void)                           /* the piece playing, into its slot */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = proj.par[p];
    proj.slot[proj.piece].pool = proj.pool;
}

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info(i)->def;
    take(&proj.slot[0]);
    proj.leds = 1;
}

static int pool_ok(uint32_t p)                   /* the root and at least two more notes */
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return (p & 1u) && !(p >> 12) && n >= 3;
}

static int pars_ok(const int16_t *par)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo || par[i] > p->hi)
            return 0;
    }
    return 1;
}

static int project_valid(void)                  /* an older format: the recipes again */
{
    int i;
    if (proj.magic != PROJ_MAGIC || !pool_ok(proj.pool) || !pars_ok(proj.par))
        return 0;
    if (proj.format != PROJ_FORMAT || proj.piece >= NPIECES)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pool_ok(proj.slot[i].pool) || !pars_ok(proj.slot[i].par))
            return 0;
    return 1;
}





/* Format 22 assigns new meanings to the filter-model IDs, but its byte
 * layout is identical to format 21. Translate legacy IDs instead of silently
 * interpreting a saved Moog model as a different array index. */
static void filter_from_legacy(int16_t *par)
{
    int old=par[P_FFILTERMODEL],type=par[P_FFILTERTYPE];
    if(old<0 || old>6 || type<0 || type>3)return; /* validation rejects */
    static const uint8_t models[7]={FM_FILTER_CLEAN,FM_FILTER_POLYSIX,
        FM_FILTER_SEM,FM_FILTER_SEM,FM_FILTER_JUNO,FM_FILTER_SEM,FM_FILTER_SEM};
    par[P_FFILTERMODEL]=(int16_t)models[old];
    if(old==0 && type==3) {par[P_FFILTERMODEL]=FM_FILTER_BYPASS;type=0;}
    else if(old==2)type=1;
    else if(old>=1 && old<=5)type=0;
    par[P_FFILTERTYPE]=(int16_t)type;
}

/* Format 23 reuses five retired Bass slots for Pad 2; save layout stays fixed. */
#define V22_NPARAMS 167
typedef struct {int16_t par[V22_NPARAMS];uint16_t pool;} slot_v22_t;
typedef struct {
    uint32_t magic,format;int16_t par[V22_NPARAMS];uint16_t pool;
    uint8_t leds,piece,rsv0[2];slot_v22_t slot[NPIECES];uint8_t rsv[16];
    en_evo_bank_t evo,fm_evo;en_proteus_bank_t proteus;en_ud_bank_t unitd;
} project_v22_t;
static void from_v22(const project_v22_t *old)
{
    project_defaults();proj.pool=old->pool;proj.leds=old->leds;proj.piece=old->piece;
    for(int p=0;p<V22_NPARAMS;p++)proj.par[p]=old->par[p];
    for(int i=0;i<NPIECES;i++){
        proj.slot[i].pool=old->slot[i].pool;
        for(int p=0;p<V22_NPARAMS;p++)proj.slot[i].par[p]=old->slot[i].par[p];
    }
    proj.evo=old->evo;proj.fm_evo=old->fm_evo;proj.proteus=old->proteus;proj.unitd=old->unitd;
    /* A former FM bypass becomes the initial Clean Pad 1 filter. */
    if(old->format==22u){
        if(proj.par[P_FFILTERMODEL]==0)proj.par[P_FFILTERMODEL]=FM_FILTER_CLEAN;
        for(int i=0;i<NPIECES;i++)if(proj.slot[i].par[P_FFILTERMODEL]==0)proj.slot[i].par[P_FFILTERMODEL]=FM_FILTER_CLEAN;
    }
}

/* The v17.99.5 format-20 struct is byte-exact: 141 parameters plus
 * Hallucigenia/Proteus banks, without UNIT-D parameters or saved state. */
#define V20_NPARAMS P_RUDSEED
typedef struct {int16_t par[V20_NPARAMS];uint16_t pool;} slot_v20_t;
typedef struct {
    uint32_t magic,format;int16_t par[V20_NPARAMS];uint16_t pool;
    uint8_t leds,piece,rsv0[2];slot_v20_t slot[NPIECES];uint8_t rsv[16];
    en_evo_bank_t evo,fm_evo;en_proteus_bank_t proteus;
} project_v20_t;
static void from_v20(const project_v20_t *o){
    slot_t r;int i,p;
    memset(&proj,0,sizeof proj);proj.magic=PROJ_MAGIC;proj.format=PROJ_FORMAT;
    proj.pool=o->pool;proj.leds=o->leds;proj.piece=o->piece<NPIECES?o->piece:0;
    for(i=0;i<NPIECES;i++){
        recipe(i,&r);
        for(p=0;p<P_NPARAMS;p++)proj.slot[i].par[p]=p<V20_NPARAMS?o->slot[i].par[p]:r.par[p];
        proj.slot[i].pool=o->slot[i].pool;
    }
    recipe(proj.piece,&r);
    for(p=0;p<P_NPARAMS;p++)proj.par[p]=p<V20_NPARAMS?o->par[p]:r.par[p];
    proj.evo=o->evo;proj.fm_evo=o->fm_evo;proj.proteus=o->proteus;
}

/* v17.99.4 format 19 has the complete Proteus banks but no filter/shape params. */
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

/* v17.99.3 format 18 is a strict binary prefix before the Proteus parameters. */
#define V18_NPARAMS P_RPROTLEN
typedef struct { int16_t par[V18_NPARAMS]; uint16_t pool; } slot_v18_t;
typedef struct {
    uint32_t magic, format; int16_t par[V18_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v18_t slot[NPIECES]; uint8_t rsv[16];
    en_evo_bank_t evo, fm_evo;
} project_v18_t;
static void from_v18(const project_v18_t *o)
{
    slot_t recipe_slot; int i, p;
    memset(&proj, 0, sizeof proj); proj.magic=PROJ_MAGIC; proj.format=PROJ_FORMAT;
    proj.pool=o->pool;proj.leds=o->leds;proj.piece=o->piece<NPIECES?o->piece:0;
    for (i=0;i<NPIECES;++i) {
        recipe(i,&recipe_slot);
        for (p=0;p<P_NPARAMS;++p)
            proj.slot[i].par[p]=p<V18_NPARAMS?o->slot[i].par[p]:recipe_slot.par[p];
        proj.slot[i].pool=o->slot[i].pool;
    }
    recipe(proj.piece,&recipe_slot);
    for (p=0;p<P_NPARAMS;++p)
        proj.par[p]=p<V18_NPARAMS?o->par[p]:recipe_slot.par[p];
    proj.evo=o->evo;proj.fm_evo=o->fm_evo;
}
#define V17_NPARAMS P_FMLEVEL /* v17.98: Rings Evolve bank, before independent FM */
typedef struct { int16_t par[V17_NPARAMS]; uint16_t pool; } slot_v17_t;
typedef struct {
    uint32_t magic, format; int16_t par[V17_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v17_t slot[NPIECES]; uint8_t rsv[16];
    en_evo_bank_t evo;
} project_v17_t;

static void from_v17(const project_v17_t *o)
{
    slot_t r; int i, p;
    memset(&proj, 0, sizeof proj); proj.magic = PROJ_MAGIC; proj.format = PROJ_FORMAT;
    proj.pool = o->pool; proj.leds = o->leds; proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++) proj.slot[i].par[p] = p < V17_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++) proj.par[p] = p < V17_NPARAMS ? o->par[p] : r.par[p];
    proj.evo = o->evo;
}

#define V16_NPARAMS P_RSEQMODE /* v17.94-v17.97: before Evolve */
typedef struct { int16_t par[V16_NPARAMS]; uint16_t pool; } slot_v16_t;
typedef struct {
    uint32_t magic, format; int16_t par[V16_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v16_t slot[NPIECES]; uint8_t rsv[16];
} project_v16_t;

static void from_v16(const project_v16_t *o)
{
    slot_t r; int i, p;
    memset(&proj, 0, sizeof proj); proj.magic = PROJ_MAGIC; proj.format = PROJ_FORMAT;
    proj.pool = o->pool; proj.leds = o->leds; proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++) proj.slot[i].par[p] = p < V16_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++) proj.par[p] = p < V16_NPARAMS ? o->par[p] : r.par[p];
}

#define V15_NPARAMS P_REVSPACE  /* v17.19-v17.93: before ambient controls */
typedef struct { int16_t par[V15_NPARAMS]; uint16_t pool; } slot_v15_t;
typedef struct {
    uint32_t magic, format; int16_t par[V15_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v15_t slot[NPIECES]; uint8_t rsv[16];
} project_v15_t;

static void from_v15(const project_v15_t *o)
{
    slot_t r; int i, p;
    memset(&proj, 0, sizeof proj); proj.magic = PROJ_MAGIC; proj.format = PROJ_FORMAT;
    proj.pool = o->pool; proj.leds = o->leds; proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++) proj.slot[i].par[p] = p < V15_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++) proj.par[p] = p < V15_NPARAMS ? o->par[p] : r.par[p];
}

#define V14_NPARAMS P_PADRANGE  /* v17.15-v17.18: before sequencer octave controls */
typedef struct { int16_t par[V14_NPARAMS]; uint16_t pool; } slot_v14_t;
typedef struct {
    uint32_t magic, format; int16_t par[V14_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v14_t slot[NPIECES]; uint8_t rsv[16];
} project_v14_t;

static void from_v14(const project_v14_t *o)
{
    slot_t r; int i, p;
    memset(&proj, 0, sizeof proj); proj.magic = PROJ_MAGIC; proj.format = PROJ_FORMAT;
    proj.pool = o->pool; proj.leds = o->leds; proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++) proj.slot[i].par[p] = p < V14_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++) proj.par[p] = p < V14_NPARAMS ? o->par[p] : r.par[p];
}

#define V13_NPARAMS P_RBURST  /* v17.14: before Rings Burst was appended */
typedef struct { int16_t par[V13_NPARAMS]; uint16_t pool; } slot_v13_t;
typedef struct {
    uint32_t magic, format; int16_t par[V13_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v13_t slot[NPIECES]; uint8_t rsv[16];
} project_v13_t;

static void from_v13(const project_v13_t *o)
{
    slot_t r; int i, p;
    memset(&proj, 0, sizeof proj); proj.magic = PROJ_MAGIC; proj.format = PROJ_FORMAT;
    proj.pool = o->pool; proj.leds = o->leds; proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++) proj.slot[i].par[p] = p < V13_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++) proj.par[p] = p < V13_NPARAMS ? o->par[p] : r.par[p];
}

#define V12_NPARAMS P_RDELAYREPEATS  /* v17.12/v17.13: before delay Repeats and Burst were appended */
typedef struct { int16_t par[V12_NPARAMS]; uint16_t pool; } slot_v12_t;
typedef struct {
    uint32_t magic, format; int16_t par[V12_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v12_t slot[NPIECES]; uint8_t rsv[16];
} project_v12_t;

static void from_v12(const project_v12_t *o)
{
    slot_t r; int i, p;
    memset(&proj, 0, sizeof proj); proj.magic = PROJ_MAGIC; proj.format = PROJ_FORMAT;
    proj.pool = o->pool; proj.leds = o->leds; proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++) proj.slot[i].par[p] = p < V12_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++) proj.par[p] = p < V12_NPARAMS ? o->par[p] : r.par[p];
}

#define V11_NPARAMS P_PADRANGE  /* v17.10: delay + Reverse + Chance; last two are discarded */
typedef struct { int16_t par[V11_NPARAMS]; uint16_t pool; } slot_v11_t;
typedef struct {
    uint32_t magic, format; int16_t par[V11_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v11_t slot[NPIECES]; uint8_t rsv[16];
} project_v11_t;

static void from_v11(const project_v11_t *o)
{
    slot_t r; int i, p;
    memset(&proj, 0, sizeof proj); proj.magic = PROJ_MAGIC; proj.format = PROJ_FORMAT;
    proj.pool = o->pool; proj.leds = o->leds; proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++) proj.slot[i].par[p] = p < P_RDELAYREPEATS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++) proj.par[p] = p < P_RDELAYREPEATS ? o->par[p] : r.par[p];
}

#define V10_NPARAMS P_RDELAYREPEATS     /* v17.9: before Repeats/Burst */
typedef struct { int16_t par[V10_NPARAMS]; uint16_t pool; } slot_v10_t;
typedef struct {
    uint32_t magic, format; int16_t par[V10_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v10_t slot[NPIECES]; uint8_t rsv[16];
} project_v10_t;

static void from_v10(const project_v10_t *o)
{
    slot_t r; int i, p;
    memset(&proj, 0, sizeof proj); proj.magic = PROJ_MAGIC; proj.format = PROJ_FORMAT;
    proj.pool = o->pool; proj.leds = o->leds; proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++) proj.slot[i].par[p] = p < V10_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++) proj.par[p] = p < V10_NPARAMS ? o->par[p] : r.par[p];
}


#define V9_NPARAMS P_RDELAYTIME
typedef struct { int16_t par[V9_NPARAMS]; uint16_t pool; } slot_v9_t;
typedef struct {
    uint32_t magic, format; int16_t par[V9_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v9_t slot[NPIECES]; uint8_t rsv[16];
} project_v9_t;

static void from_v9(const project_v9_t *o)
{
    slot_t r; int i, p;
    memset(&proj, 0, sizeof proj); proj.magic = PROJ_MAGIC; proj.format = PROJ_FORMAT;
    proj.pool = o->pool; proj.leds = o->leds; proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++) proj.slot[i].par[p] = p < V9_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++) proj.par[p] = p < V9_NPARAMS ? o->par[p] : r.par[p];
}

#define V8_NPARAMS P_PAD2OCT
typedef struct { int16_t par[V8_NPARAMS]; uint16_t pool; } slot_v8_t;
typedef struct {
    uint32_t magic, format; int16_t par[V8_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v8_t slot[NPIECES]; uint8_t rsv[16];
} project_v8_t;

static void from_v8(const project_v8_t *o)
{
    slot_t r; int i,p;
    memset(&proj,0,sizeof proj); proj.magic=PROJ_MAGIC; proj.format=PROJ_FORMAT;
    proj.pool=o->pool; proj.leds=o->leds; proj.piece=o->piece<NPIECES?o->piece:0;
    for(i=0;i<NPIECES;i++){
        recipe(i,&r);
        for(p=0;p<P_NPARAMS;p++) proj.slot[i].par[p]=p<V8_NPARAMS?o->slot[i].par[p]:r.par[p];
        proj.slot[i].par[P_HITS] = proj.slot[i].par[P_SWELL];
        proj.slot[i].par[P_SWING] = proj.slot[i].par[P_BRIGHT];
        proj.slot[i].par[P_HATS] = proj.slot[i].par[P_BTONE];
        proj.slot[i].par[P_DRONEVOICE] = proj.slot[i].par[P_PADTONE];
        proj.slot[i].par[P_DRONEREV] = proj.slot[i].par[P_PADREV];
        proj.slot[i].par[P_PAD2OCT] = proj.slot[i].par[P_PADOCT];
        proj.slot[i].par[P_PAD2CHAR] = proj.slot[i].par[P_PADCHAR];
        proj.slot[i].par[P_PAD2CHORUS] = proj.slot[i].par[P_PADCHORUS];
        proj.slot[i].pool=o->slot[i].pool;
    }
    recipe(proj.piece,&r);
    for(p=0;p<P_NPARAMS;p++) proj.par[p]=p<V8_NPARAMS?o->par[p]:r.par[p];
    proj.par[P_HITS] = proj.par[P_SWELL];
    proj.par[P_SWING] = proj.par[P_BRIGHT];
    proj.par[P_HATS] = proj.par[P_BTONE];
    proj.par[P_DRONEVOICE] = proj.par[P_PADTONE];
    proj.par[P_DRONEREV] = proj.par[P_PADREV];
    proj.par[P_PAD2OCT] = proj.par[P_PADOCT];
    proj.par[P_PAD2CHAR] = proj.par[P_PADCHAR];
    proj.par[P_PAD2CHORUS] = proj.par[P_PADCHORUS];
}

#define V7_NPARAMS P_DRONEVOICE
typedef struct { int16_t par[V7_NPARAMS]; uint16_t pool; } slot_v7_t;
typedef struct {
    uint32_t magic, format; int16_t par[V7_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v7_t slot[NPIECES]; uint8_t rsv[16];
} project_v7_t;

static void from_v7(const project_v7_t *o)
{
    slot_t r; int i,p;
    memset(&proj,0,sizeof proj); proj.magic=PROJ_MAGIC; proj.format=PROJ_FORMAT;
    proj.pool=o->pool; proj.leds=o->leds; proj.piece=o->piece<NPIECES?o->piece:0;
    for(i=0;i<NPIECES;i++){ recipe(i,&r); for(p=0;p<P_NPARAMS;p++) proj.slot[i].par[p]=p<V7_NPARAMS?o->slot[i].par[p]:r.par[p]; proj.slot[i].pool=o->slot[i].pool; }
    recipe(proj.piece,&r); for(p=0;p<P_NPARAMS;p++) proj.par[p]=p<V7_NPARAMS?o->par[p]:r.par[p];
}

#define V6_NPARAMS P_PADCHAR
typedef struct { int16_t par[V6_NPARAMS]; uint16_t pool; } slot_v6_t;
typedef struct {
    uint32_t magic, format; int16_t par[V6_NPARAMS]; uint16_t pool;
    uint8_t leds, piece, rsv0[2]; slot_v6_t slot[NPIECES]; uint8_t rsv[16];
} project_v6_t;

static void from_v6(const project_v6_t *o)
{
    slot_t r; int i,p;
    memset(&proj,0,sizeof proj); proj.magic=PROJ_MAGIC; proj.format=PROJ_FORMAT;
    proj.pool=o->pool; proj.leds=o->leds; proj.piece=o->piece<NPIECES?o->piece:0;
    for(i=0;i<NPIECES;i++){ recipe(i,&r); for(p=0;p<P_NPARAMS;p++) proj.slot[i].par[p]=p<V6_NPARAMS?o->slot[i].par[p]:r.par[p]; proj.slot[i].pool=o->slot[i].pool; }
    recipe(proj.piece,&r); for(p=0;p<P_NPARAMS;p++) proj.par[p]=p<V6_NPARAMS?o->par[p]:r.par[p];
}
/* format 3 (0.9 before the layer pages): 23 settings a piece. Carried over: each piece's own, the
 * new ones from its recipe */
#define V3_NPARAMS (P_PADTONE + 1)
typedef struct {
    int16_t par[V3_NPARAMS];
    uint16_t pool;
} slot_v3_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V3_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v3_t slot[NPIECES];
    uint8_t rsv[16];
} project_v3_t;

static void from_v3(const project_v3_t *o)
{
    slot_t r;
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < V3_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < V3_NPARAMS ? o->par[p] : r.par[p];
}

int project_load(void)
{
    static union {
        project_t now;
        project_v22_t v22;
        project_v20_t v20;
        project_v19_t v19;
        project_v18_t v18;
        project_v17_t v17;
        project_v16_t v16;
        project_v15_t v15;
        project_v14_t v14;
        project_v13_t v13;
        project_v12_t v12;
        project_v11_t v11;
        project_v10_t v10;
        project_v9_t v9;
        project_v8_t v8;
        project_v7_t v7;
        project_v6_t v6;
        project_v3_t v3;
    } buf;
    int n, i;
    memset(&buf, 0, sizeof buf);
    n = plat_store_load(OBJ_PROJ, &buf, sizeof buf);
    if(n>=(int)sizeof(project_v22_t) && buf.now.magic==PROJ_MAGIC && (buf.now.format==21u || buf.now.format==22u))from_v22(&buf.v22);
    else if (n >= (int)sizeof(project_v20_t) && buf.now.magic == PROJ_MAGIC && buf.now.format == 20u)
        from_v20(&buf.v20);
    else if (n >= (int)sizeof(project_v19_t) && buf.now.magic == PROJ_MAGIC && buf.now.format == 19u)
        from_v19(&buf.v19);
    else if (n >= (int)sizeof(project_v18_t) && buf.now.magic == PROJ_MAGIC && buf.now.format == 18u)
        from_v18(&buf.v18);
    else if (n >= (int)sizeof(project_v17_t) && buf.now.magic == PROJ_MAGIC && buf.now.format == 17u)
        from_v17(&buf.v17);
    else if (n >= (int)sizeof(project_v16_t) && buf.now.magic == PROJ_MAGIC && buf.now.format == 16u)
        from_v16(&buf.v16);
    else if (n >= (int)sizeof(project_v15_t) && buf.now.magic == PROJ_MAGIC && buf.now.format == 15u)
        from_v15(&buf.v15);
    else if (n >= (int)sizeof(project_v14_t) && buf.now.magic == PROJ_MAGIC && buf.now.format == 14u)
        from_v14(&buf.v14);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 13u)
        from_v13(&buf.v13);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 12u)
        from_v12(&buf.v12);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 11u)
        from_v11(&buf.v11);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 10u)
        from_v10(&buf.v10);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 9u)
        from_v9(&buf.v9);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 8u)
        from_v8(&buf.v8);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 7u)
        from_v7(&buf.v7);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 6u)
        from_v6(&buf.v6);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 3u)
        from_v3(&buf.v3);
    else if (n >= (int)sizeof(project_t))
        proj = buf.now;
    else {
        project_defaults();
        return -1;
    }
    if(n>=8 && buf.now.magic==PROJ_MAGIC &&
       (buf.now.format==20u || buf.now.format==21u)) {
        filter_from_legacy(proj.par);
        for(i=0;i<NPIECES;i++)filter_from_legacy(proj.slot[i].par);
        proj.format=PROJ_FORMAT;
    }
    /* Reclaim only the five removed Bass slots when upgrading to format 23. */
    if(n>=8 && buf.now.magic==PROJ_MAGIC && buf.now.format<23u){
        const int ids[]={P_P2FILTERMODEL,P_P2FILTERTYPE,P_P2FILTERCUT,P_P2FILTERRES,P_P2FILTERMIX};
        for(int row=-1;row<NPIECES;row++){
            int16_t *p=row<0?proj.par:proj.slot[row].par;
            for(int j=0;j<5;j++)p[ids[j]]=en_param_info(ids[j])->def;
        }
    }
    /* Same format/layout: preserve old projects, capping only formerly valid
     * 7..10 voice settings. Values outside the old range remain invalid. */
    if (proj.magic == PROJ_MAGIC && proj.format == PROJ_FORMAT) {
        for (i = -1; i < NPIECES; ++i) {
            int16_t *p = i < 0 ? proj.par : proj.slot[i].par;
            if (p[P_FMPOLY] > FM_PORT_VOICES && p[P_FMPOLY] <= 10)
                p[P_FMPOLY] = FM_PORT_VOICES;
        }
    }
    if (n >= 8 && project_valid()) {
        /* Map removed models to a supported counterpart only after validation,
         * so corrupt IDs still fail load. Supported stored IDs stay unchanged.
         */
        proj.par[P_RMODEL] = (int16_t)rings_model_normalize(proj.par[P_RMODEL]);
        for (i = 0; i < NPIECES; i++)
            proj.slot[i].par[P_RMODEL] = (int16_t)rings_model_normalize(proj.slot[i].par[P_RMODEL]);
        return 0;
    }
    project_defaults();
    return -1;
}

int project_save(void)
{
    keep();
    en_evo_snapshot(&proj.evo);
    en_fmevo_snapshot(&proj.fm_evo);
    en_proteus_snapshot(&proj.proteus);
    en_unitd_snapshot(&proj.unitd);
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece]);
}

void project_piece_reset(void)
{
    slot_t s;
    recipe(proj.piece, &s);
    take(&s);
}

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    en_pool(proj.pool);
}
