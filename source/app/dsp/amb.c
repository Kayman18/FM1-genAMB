/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument (amb.h). One compilation unit, -O2. */
#include "amb.h"
#include "fastmath.h"
#include "rings_port.h"
#include "monumatic_reverb_port.h"
#include "rings_models.h"
#include "fm_port.h"
#include "fm_filter.h"
#include "dual_core.h"
#include "evolve_rate.h"
#include "octave_pitch.h"

#if defined EN_HOST || defined PLAT_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const EN_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *const EN_MODE_NAME[EN_NMODES] = {"Ionian", "Dorian", "Phrygian", "Lydian", "Mixolyd.", "Aeolian",
                                             "Pent maj", "Pent min", "Piano"};
#define B(a, b, c, d, e, f, g) (1u << (a) | 1u << (b) | 1u << (c) | 1u << (d) | 1u << (e) | 1u << (f) | 1u << (g))
const uint16_t EN_MODE_POOL[EN_NMODES] = {
    B(0, 2, 4, 5, 7, 9, 11), B(0, 2, 3, 5, 7, 9, 10), B(0, 1, 3, 5, 7, 8, 10), B(0, 2, 4, 6, 7, 9, 11),
    B(0, 2, 4, 5, 7, 9, 10), B(0, 2, 3, 5, 7, 8, 10), B(0, 2, 4, 7, 9, 9, 9),  B(0, 3, 5, 7, 10, 10, 10),
    0xFFFu}; /* PIANO keeps the current project pool; this entry is only a safe fallback. */
#undef B
const uint8_t EN_LOOP_LEN[EN_NLOOP] = {13, 17, 23, 29};
const uint8_t EN_DRUM_N[EN_NDRUM] = {16, 12, 7};

static int16_t par[P_NPARAMS];
static const en_param_t PARAMS[P_NPARAMS] = {
    {"Pad", 0, 100, 60},      {"Bells", 0, 100, 65},    {"Pad 2 model",0,5,1},     {"Pad 2", 0, 100, 40},
    {"Density", 0, 100, 40},  {"Evolve", 0, 100, 35},   {"Drift", 0, 100, 25},    {"Swell 2", 0, 100, 60},
    {"Reserved damp", 0, 100, 45},  {"Rev motion", 0, 100, 100}, {"Reverb", 0, 100, 65},   {"Decay", 0, 100, 80},
    {"Bright", 0, 100, 40},   {"Swell", 0, 100, 60},    {"Low body", 0, 100, 38}, {"Motion", 0, 100, 50},
    {"Tune", -50, 50, 0},     {"MIDI out", 0, 1, 1},    {"Tempo", 30, 120, 72},   {"Root", 0, 11, 2},
    {"Mode", 0, EN_NMODES - 1, 1}, {"Rings oct", -4, 2, 0}, {"Pad tone", 0, 1, 0},
    {"Pad 2 type",0,3,0},   {"Pad 2 cutoff",0,100,78},    {"Pad 2 res",0,100,18},
    {"Bright 2", 0, 100, 40}, {"Low body 2", 0, 100, 38},
    {"R model", 0, 6, 0},      {"Structure", 0, 100, 45},
    {"R bright", 0, 100, 55},  {"Damping", 0, 100, 65},
    {"Position", 0, 100, 35},  {"Polyphony", 1, 4, 4},
    {"String FX", 0, 5, 4},
    {"M1 depth", 0, 100, 45}, {"M1 speed", 0, 100, 30},
    {"M2 depth", 0, 100, 45}, {"M2 speed", 0, 100, 30},
    {"M3 depth", 0, 100, 45}, {"M3 speed", 0, 100, 30},
    {"M4 depth", 0, 100, 45}, {"M4 speed", 0, 100, 30},
    {"M5 depth", 0, 100, 45}, {"M5 speed", 0, 100, 30},
    {"M6 depth", 0, 100, 45}, {"M6 speed", 0, 100, 30},
    {"M7 dest", 0, 14, 1},      {"M7 depth", -100, 100, 25}, {"M7 speed", 0, 100, 25},
    {"M8 dest", 0, 14, 2},      {"M8 depth", -100, 100, 25}, {"M8 speed", 0, 100, 25},
    {"M9 dest", 0, 14, 3},     {"M9 depth", -100, 100, 20}, {"M9 speed", 0, 100, 20},
    {"Character", 0, 100, 50},   {"Pad rev", 0, 100, 65}, {"Rings rev", 0, 100, 75},
    {"Pad 2 rev", 0, 100, 65},   {"Pad 2 mix",0,100,100},
    {"Pad2 tone", 0, 1, 0},        {"Exciter", 0, 100, 55},       {"Chorus", 0, 100, 28},
    {"Pad oct", -4, 2, 0},
    {"Pad2 oct", -4, 2, 0},        {"Character 2", 0, 100, 50},  {"Width 2", 0, 100, 28},
    {"R delay", 0, 100, 33},          {"R delay fb", 0, 85, 35},    {"R delay mix", 0, 100, 0},
    {"R repeats", 0, 8, 0}, /* 0 = infinite/feedback-only, 1..8 = audible repeats */
    {"Burst", 0, 100, 0},      /* chance that a generated Rings strike blooms into a short chord strum */
    {"Oct Drift", 0, 2, 0}, {"Change", 0, 100, 0},
    {"Oct Drift", 0, 2, 0}, {"Change", 0, 100, 0},
    {"Oct Drift", 0, 2, 0}, {"Change", 0, 100, 0},
    {"Reverb type", 0, 9, 0}, {"Diffusion", 0, 100, 55},
    {"Dly motion", 0, 100, 30}, {"Delay rev", 0, 100, 65},
    {"Seq mode",0,3,0}, {"Length",2,16,16}, {"Mutation",0,100,15}, {"Density",0,100,70},
    {"Rate",-1,11,1}, {"Range",1,3,1}, {"Follow",0,1,1}, {"Pattern",1,16,1},
    {"FM level",0,100,0}, {"FM patch",0,FM_PORT_PATCHES-1,0}, {"FM voices",1,FM_PORT_VOICES,FM_PORT_DEFAULT_VOICES}, {"FM gate",10,200,75}, {"FM reverb",0,100,65},
    {"FM seq",0,3,0}, {"FM octave",-4,2,0}, {"Oct Drift",0,2,0}, {"Chance",0,100,0},
    {"FM density",0,100,40}, {"FM burst",0,100,0},
    {"FM length",2,16,16}, {"FM mutate",0,100,15}, {"FM dens",0,100,70},
    {"FM rate",-1,11,1}, {"FM range",1,3,1}, {"FM follow",0,1,1}, {"FM pattern",1,16,1},
    /* Proteus uses 32-note sequences and the source module's Poisson patience. */
    {"Length",1,32,16}, {"Density",1,100,80}, {"Patience",1,50,12}, {"Rate",-1,11,1},
    {"Gate",5,100,50}, {"Oct Prob",0,100,20}, {"Mutate",0,100,20}, {"Lock",0,2,0}, {"Range",0,2,1},
    {"Length",1,32,16}, {"Density",1,100,80}, {"Patience",1,50,12}, {"Rate",-1,11,1},
    {"Gate",5,100,50}, {"Oct Prob",0,100,20}, {"Mutate",0,100,20}, {"Lock",0,2,0}, {"Range",0,2,1},
    /* Model 0 is Standard; Type 3=Bypass keeps old v19 projects byte identical. */
    {"Pad 1 model",0,5,1}, {"Filter type",0,3,0}, {"Cutoff",0,100,78},
    {"Resonance",0,100,25}, {"Filter mix",0,100,100},
    {"M7 shape",0,5,0}, {"M8 shape",0,5,0}, {"M9 shape",0,5,0},
    {"M10 depth",-100,100,65}, {"M10 seconds",0,180,90}, {"M10 shape",0,5,2},
    {"M11 depth",-100,100,40}, {"M11 seconds",0,180,135}, {"M11 shape",0,5,4},
    /* UNIT-D: Shape/Traversal/Gates/Options. Both layers have independent values. */
    {"Seed",0,999,42},{"Nodes",8,64,24},{"Radius",25,200,100},{"Density",0,100,60},
    {"Walk",0,2,1},{"Lock",-100,100,0},{"Gate Len",5,95,45},{"Gate Den",15,100,75},
    {"Tolerance",1,20,4},{"Range",1,4,2},{"Rate",-1,11,1},{"Polyphony",1,4,1},
    {"Seed",0,999,42},{"Nodes",8,64,24},{"Radius",25,200,100},{"Density",0,100,60},
    {"Walk",0,2,1},{"Lock",-100,100,0},{"Gate Len",5,95,45},{"Gate Den",15,100,75},
    {"Tolerance",1,20,4},{"Range",1,4,2},{"Rate",-1,11,1},{"Polyphony",1,8,1},
    /* Append-only IDs retained: Tone is active, former GP5 rate is reserved. */
    {"Rev tone",0,100,85},{"Reserved rate",1,500,100},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

/* the pieces: eight recipes for the whole garden (the FM-1's PRESETS, the web app's cards). Tune and
 * MIDI out are taken from here by neither: they belong to the player, not the piece */
/*                     Pad Bel Bas Pul  Den Evo Dri Hit  Ech ET Rev Siz  Bri Swe Ton Mot  Tun MI Tem Rt Md Oc  PT  BO Gli Fif  Swi Hat
 * (PT: the pad's colour, 0 Rhodes, 1 Vintage Analog; BO: the bass's octave) */
static const struct {
    const char *name;
    int8_t v[P_NPARAMS];
} PIECES[EN_NPIECES] = {
    {"Garden",       {60, 65, 45, 40, 40, 35, 25, 35, 45, 3, 65, 80, 40, 60, 38, 50, 0, 1, 72, 2, 1, 0, 0, 0, 20, 35, 0, 50}},
    {"Departures",   {35, 75, 0, 0, 35, 30, 40, 0, 30, 3, 70, 85, 20, 55, 38, 40, 0, 1, 60, 1, 0, -1, 0, 0, 30, 25, 0, 40}},
    {"Voices",       {85, 0, 25, 0, 30, 30, 50, 0, 20, 5, 80, 90, 67, 70, 38, 55, 0, 1, 56, 5, 5, 0, 1, -1, 45, 30, 0, 40}},
    {"Afternoon",    {50, 65, 35, 25, 55, 45, 20, 30, 50, 2, 60, 75, 60, 50, 38, 55, 0, 1, 84, 7, 3, 0, 0, 0, 15, 40, 15, 65}},
    {"Night Bus",    {45, 55, 55, 60, 45, 50, 15, 55, 55, 1, 55, 70, 35, 40, 38, 70, 0, 1, 96, 9, 5, 0, 0, 0, 10, 45, 30, 70}},
    {"Shoreline",    {40, 60, 30, 0, 30, 40, 40, 0, 85, 4, 55, 95, 30, 80, 38, 85, 0, 1, 64, 4, 6, 0, 0, 0, 40, 20, 0, 30}},
    {"Glass",        {35, 80, 30, 20, 60, 55, 25, 25, 45, 2, 65, 80, 55, 45, 38, 50, 0, 1, 76, 3, 7, 1, 0, 1, 10, 30, 10, 85}},
    {"Tides",        {55, 60, 40, 35, 45, 75, 10, 40, 50, 3, 65, 85, 60, 60, 38, 95, 0, 1, 70, 10, 4, 0, 1, 0, 25, 55, 10, 50}},
};

const char *en_piece_name(int i) { return PIECES[(unsigned)i < EN_NPIECES ? i : 0].name; }
void en_piece(int i, int16_t *par, uint16_t *pool)
{
    int p;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    for (p = 0; p < P_NPARAMS; p++)
        par[p] = p < P_RMODEL ? PIECES[i].v[p] : PARAMS[p].def;
    /* Echo time belonged to the removed tape delay; it is now plate modulation depth.
     * Old piece recipes stored 0..5 here, so use the new useful default instead. */
    par[P_ETIME] = PARAMS[P_ETIME].def;
    /* Pad 2 starts as a timbral duplicate of Pad 1. Its state/timing is independent
     * once running; only the initial recipe values are copied. */
    par[P_HITS] = par[P_SWELL];
    par[P_SWING] = par[P_BRIGHT];
    par[P_HATS] = par[P_BTONE];
    par[P_DRONEVOICE] = par[P_PADTONE];
    par[P_DRONEREV] = par[P_PADREV];
    par[P_PAD2OCT] = par[P_PADOCT];
    par[P_PAD2CHAR] = par[P_PADCHAR];
    par[P_PAD2CHORUS] = par[P_PADCHORUS];
    const int f2[5]={P_P2FILTERMODEL,P_P2FILTERTYPE,P_P2FILTERCUT,P_P2FILTERRES,P_P2FILTERMIX};
    for(int j=0;j<5;j++)par[f2[j]]=PARAMS[f2[j]].def;
    *pool = EN_MODE_POOL[PIECES[i].v[P_MODE]];
}

static const char *const ETIME_NAME[6] = {"1/4.", "1/2", "1/2.", "1 bar", "2 bars", "4 bars"};
static const float ETIME_BEATS[6] = {1.5f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f};

static char *itoa_s(int v, char *b)
{
    char t[8];
    int n = 0, neg = v < 0;
    if (neg)
        v = -v;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 6);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}

static float drift_secs(int v) { return 20.0f * fm_exp2f((float)v * 0.039069f); }   /* 20 s .. 5 min */
static float mod_secs_from_speed(int v) { return 1200.0f * fm_exp2f(-6.321928f * (float)v * 0.01f); }

void en_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    const char *s = 0;
    if (i == P_RSEQMODE || i == P_FMSEQMODE)
        s = v == 3 ? "Unit-D" : v == 2 ? "Proteus" : v == 1 ? "Hallucigenia" : "Original";
    else if (i == P_RPROTLOCK || i == P_FPROTLOCK)
        s = v == 2 ? "Locked" : v == 1 ? "No Mutate" : "Auto";
    else if (i == P_RPROTRANGE || i == P_FPROTRANGE)
        s = v == 2 ? "+/-2" : v == 1 ? "+/-1" : "Fixed";
    else if (i == P_RUDWALK || i == P_FUDWALK) {
        static const char *const nm[]={"Forward","Random","Degree"};
        s=nm[(unsigned)v<3u?v:0];
    } else if (i == P_RUDLOCK || i == P_FUDLOCK) {
        if(v == 0) s="Free";
        else {
            b[0]=(v<0)?'3':'1';b[1]=(v<0)?'2':'6';b[2]=':';
            itoa_s(v<0?-v:v,b+3);return;
        }
    }
    else if (i == P_FMPATCH) s = fm_port_patch_name(v);
    else if ((i == P_FFILTERMODEL || i==P_P2FILTERMODEL)) {
        static const char *const names[]={"Bypass","Clean SVF","SEM","Juno","Polysix","CS-80"};
        s=names[(unsigned)v<6u?v:0];
    } else if ((i == P_FFILTERTYPE || i==P_P2FILTERTYPE)) {
        static const char *const names[]={"Low Pass","High Pass","Band Pass","Notch"};
        int model=par[i==P_P2FILTERTYPE?P_P2FILTERMODEL:P_FFILTERMODEL];
        s=names[(unsigned)v<4u?v:0];
        if(model==FM_FILTER_BYPASS)s="Bypass";
        if(model==FM_FILTER_JUNO || model==FM_FILTER_POLYSIX)s="Low Pass";
        if(model==FM_FILTER_CS80)s="HP+LP";
    } else if (i==P_M7SHAPE || i==P_M8SHAPE || i==P_M9SHAPE || i==P_M10SHAPE || i==P_M11SHAPE) {
        static const char *const nm[]={"Smooth Rnd","Step Rnd","Sine","Pulse","Triangle","Saw"};
        s=nm[(unsigned)v<6u?v:0];
    }
    else if (i == P_REVSPACE) {
        s = "Monumatic";
    } else if (i == P_GP5RATE) {
        /* Poly GP5 rate: raw value * 0.05 Hz, display with two decimals. */
        int hz100 = v * 5;
        b = itoa_s(hz100 / 100, b); *b++='.'; *b++=(char)('0'+(hz100/10)%10); *b++=(char)('0'+hz100%10); *b++=' '; *b++='H'; *b++='z'; *b=0; return;
    } else if (i == P_EFOLLOW || i == P_FMEFOLLOW) s = v ? "Chord" : "Scale";
    else if (i == P_ERATE || i == P_FMERATE || i == P_RPROTRATE || i == P_FPROTRATE || i == P_RUDRATE || i == P_FUDRATE) { s=seq_rate_label(v); }
    else if (i == P_RUDRADIUS || i == P_FUDRADIUS || i == P_RUDTOL || i == P_FUDTOL) {
        /* Fixed-point decimal values keep the original UNIT-D ranges. */
        int x=v/100, y=v%100;
        if(i==P_RUDTOL || i==P_FUDTOL) x=0;
        b[0]=(char)('0'+x);b[1]='.';b[2]=(char)('0'+y/10);b[3]=(char)('0'+y%10);b[4]=0;return;
    }
    else if (i == P_PADRANGE || i == P_PAD2RANGE || i == P_RINGRANGE || i == P_FMRANGE) {
        static const char *const range[] = {"0", "+1", "+/-1"};
        s = range[(unsigned)v < 3 ? v : 0];
    } else if (i == P_MIDI)
        s = ONOFF[v ? 1 : 0];
    else if (i == P_PADTONE)
        s = v ? "Vintage" : "Rhodes";
    else if (i == P_DRONEVOICE)
        s = v ? "Strings" : "Glass";
    else if (i == P_MODE)
        s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0];
    else if (i == P_ROOT)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    else if (i == P_RMODEL) {
        static const char *const N[RINGS_MODEL_COUNT] = {"Modal", "Sympat.", "String"};
        s = N[rings_model_index(v)];
    } else if (i == P_RFX) {
        static const char *const N[] = {"Formant", "Chorus", "Reverb", "Formant2", "Ensemble", "Reverb2"};
        s = N[(unsigned)v < 6u ? v : 0];
    } else if (i == P_M7DEST || i == P_M8DEST || i == P_M9DEST) {
        static const char *const N[] = {"Off", "Struct", "Bright", "Damp", "Position", "Pitch", "Model",
                                       "R Level", "Pad2 Bright", "Pad2 Body", "Pad Body", "Pad Char", "Rev Size", "Rev Damp", "Exciter"};
        s = N[(unsigned)v < 15u ? v : 0];
    }
    if (s) {
        int k = 0;
        while (s[k] && k < 9) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if (i == P_EMUTATE || i == P_EDENSITY || i == P_RBURST || i == P_PADCHANCE || i == P_PAD2CHANCE || i == P_RINGCHANCE ||
        i == P_FMEMUTATE || i == P_FMEDENSITY || i == P_FMDENSITY || i == P_FMBURST || i == P_FMCHANCE || i == P_FMGATE) {
        b = itoa_s(v, b); *b++ = '%'; *b = 0; return;
    }
    if (i == P_ERANGE || i == P_FMERANGE) { b=itoa_s(v,b); *b++=' '; *b++='o'; *b++='c'; *b++='t'; *b=0; return; }
    if (i == P_RDELAYREPEATS) {
        if (v <= 0) { b[0]='I'; b[1]='n'; b[2]='f'; b[3]=0; return; }
        b = itoa_s(v, b); *b = 0; return;
    }
    if (i == P_RDELAYTIME) {
        int ms = v <= 80 ? 20 + (v * 780 + 50) / 100
                         : 644 + ((v - 80) * 856 + 10) / 20;
        b = itoa_s(ms, b); *b++ = 'm'; *b++ = 's'; *b = 0;
        return;
    }
    if (i == P_M10SPEED || i == P_M11SPEED) {
        b=itoa_s(v,b); *b++='s'; *b=0;return;
    }
    if ((i == P_FFILTERCUT || i==P_P2FILTERCUT)) {
        /* Cutoff is logarithmic from 20 Hz to about 17.5 kHz. */
        int hz=(int)(20.0f*fm_exp2f(0.0978f*(float)v)+0.5f);
        if(hz>=1000){
            b=itoa_s(hz/1000,b);*b++='.';*b++=(char)('0'+(hz%1000)/100);*b++='k';
        }else{b=itoa_s(hz,b);*b++='H';*b++='z';}
        *b=0;return;
    }
    if (i == P_M1SPEED || i == P_M2SPEED || i == P_M3SPEED || i == P_M4SPEED || i == P_M5SPEED || i == P_M6SPEED ||
        i == P_M7SPEED || i == P_M8SPEED || i == P_M9SPEED) {
        int sec = (int)(mod_secs_from_speed(v) + 0.5f);
        if (sec >= 60) {
            b = itoa_s(sec / 60, b); *b++ = 'm';
        } else {
            b = itoa_s(sec, b); *b++ = 's';
        }
        *b = 0;
        return;
    }
    if (i == P_DRIFT) {
        int sec = (int)(drift_secs(v) + 0.5f);
        if (sec < 60) {
            b = itoa_s(sec, b);
            *b++ = 's';
            *b = 0;
        } else {
            b = itoa_s(sec / 60, b);
            *b++ = ':';
            *b++ = (char)('0' + sec % 60 / 10);
            *b++ = (char)('0' + sec % 10);
            *b = 0;
        }
        return;
    }
    if ((i == P_TUNE || i == P_BOCT || i == P_PADOCT || i == P_PAD2OCT) && v > 0)
        *b++ = '+';
    itoa_s(v, b);
}

/* ------------------------------------------------------- the pure parts --- */
uint32_t en_euclid(int k, int n, int rot)
{
    /* Bjorklund's algorithm (as Toussaint's "The Euclidean Algorithm Generates Traditional Musical
     * Rhythms"): k groups "x" and n - k groups ".", the remainders folded onto the heads until at most
     * one remains. Groups are bit strings, LSB first: (bits, length). */
    uint32_t gb[32], m = 0;
    uint8_t gl[32];
    int ng, heads, tails, i, pos = 0;
    if (n <= 0 || n > 32 || k <= 0)
        return 0;
    if (k > n)
        k = n;
    for (i = 0; i < n; i++) {
        gb[i] = i < k ? 1u : 0u;
        gl[i] = 1;
    }
    ng = n;
    heads = k;
    tails = n - k;
    while (tails > 1 && heads > 0) {
        int pairs = heads < tails ? heads : tails;
        for (i = 0; i < pairs; i++) {           /* a tail onto each head */
            gb[i] |= gb[ng - pairs + i] << gl[i];
            gl[i] = (uint8_t)(gl[i] + gl[ng - pairs + i]);
        }
        if (heads > tails) {                    /* the unpaired heads become the new tails */
            tails = heads - pairs;
            heads = pairs;
        } else {
            tails -= pairs;
            heads = pairs;
        }
        ng = heads + tails;
    }
    for (i = 0; i < ng; i++) {
        uint32_t j;
        for (j = 0; j < gl[i]; j++)
            if (gb[i] >> j & 1u)
                m |= 1u << (((pos + (int)j + rot) % n + n) % n);
        pos += gl[i];
    }
    return m;
}

int en_scale(uint32_t pool, int *steps)
{
    int i, m = 0;
    pool |= 1u;
    for (i = 0; i < 12; i++)
        if (pool >> i & 1u)
            steps[m++] = i;
    return m;
}

int en_chord_tones(uint32_t pool, int root, int deg, int *pcs)
{
    static const uint8_t OFF[7] = {0, 2, 4, 6, 1, 3, 5};   /* stacked thirds, then what's left */
    int st[12], m = en_scale(pool, st), want, n = 0, i, j;
    want = m >= 6 ? 4 : 3;
    deg = ((deg % m) + m) % m;
    for (i = 0; i < 7 && n < want; i++) {
        int pc = (root + st[(deg + OFF[i]) % m]) % 12, dup = 0;
        for (j = 0; j < n; j++)
            dup |= pcs[j] == pc;
        if (!dup)
            pcs[n++] = pc;
    }
    return n;
}

static int nearest(int from, int pc, int lo, int hi)    /* the note of class pc nearest from, in lo..hi */
{
    int n = from + ((pc - from) % 12 + 12) % 12;          /* at or above */
    if (n - from > 6)
        n -= 12;
    while (n < lo)
        n += 12;
    while (n > hi)
        n -= 12;
    return n;
}

void en_voice_lead(const int *from, const int *pcs, int nt, int *to, int lo, int hi)
{
    static const uint8_t PERM[24][4] = {
        {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 1, 2}, {0, 3, 2, 1},
        {1, 0, 2, 3}, {1, 0, 3, 2}, {1, 2, 0, 3}, {1, 2, 3, 0}, {1, 3, 0, 2}, {1, 3, 2, 0},
        {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 1, 0, 3}, {2, 1, 3, 0}, {2, 3, 0, 1}, {2, 3, 1, 0},
        {3, 0, 1, 2}, {3, 0, 2, 1}, {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}};
    int t[4], p, v, best = 1 << 30;
    for (v = 0; v < 4; v++)
        t[v] = pcs[v < nt ? v : 0];                        /* three tones: the root doubled */
    for (p = 0; p < 24; p++) {
        int c = 0, n[4], a, b;
        for (v = 0; v < 4; v++) {
            n[v] = nearest(from[v], t[PERM[p][v]], lo, hi);
            c += n[v] > from[v] ? n[v] - from[v] : from[v] - n[v];
        }
        for (a = 0; a < 4; a++)                            /* no two voices on one note */
            for (b = a + 1; b < 4; b++)
                if (n[a] == n[b])
                    c += 40;
        if (c < best) {
            best = c;
            for (v = 0; v < 4; v++)
                to[v] = n[v];
        }
    }
}

const char *en_chord_name(const int *pcs, int n, char *buf)
{
    uint32_t iv = 0;
    int i, k = 0, third, fifth;
    const char *q = "", *r = EN_NOTE_NAME[pcs[0] % 12];
    for (i = 1; i < n; i++)
        iv |= 1u << ((pcs[i] - pcs[0] + 12) % 12);
    third = iv >> 4 & 1u ? 4 : iv >> 3 & 1u ? 3 : 0;
    fifth = iv >> 7 & 1u ? 7 : iv >> 6 & 1u ? 6 : iv >> 8 & 1u ? 8 : 0;
    if (third == 4) {
        q = fifth == 8 ? "aug" : iv >> 11 & 1u ? "M7" : iv >> 10 & 1u ? "7" : iv >> 9 & 1u ? "6"
            : iv >> 2 & 1u ? "add9" : "";
    } else if (third == 3) {
        if (fifth == 6)
            q = iv >> 10 & 1u ? "m7b5" : iv >> 9 & 1u ? "dim7" : "dim";
        else
            q = iv >> 10 & 1u ? "m7" : iv >> 11 & 1u ? "mM7" : iv >> 9 & 1u ? "m6" : iv >> 2 & 1u ? "madd9" : "m";
    } else {
        q = iv >> 5 & 1u ? (iv >> 10 & 1u ? "7sus4" : "sus4") : iv >> 2 & 1u ? "sus2" : "5";
    }
    while (*r)
        buf[k++] = *r++;
    while (*q)
        buf[k++] = *q++;
    buf[k] = 0;
    return buf;
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_POOL, C_PLAY, C_NEXT, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW, C_RINGNOTE, C_EVONEW,
       C_FMNOTE, C_FMOFF, C_FMEVONEW, C_FMALLOFF, C_PROTNEW };
#define QN 192u
typedef char complete_project_fits_command_queue[(P_NPARAMS + 8 < QN) ? 1 : -1];
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                                   /* full: dropped (never, at the UI's rate) */
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void en_evo_new(void) { post(C_EVONEW,0,0); }
void en_fmevo_new(void) { post(C_FMEVONEW,0,0); }
void en_proteus_new(int layer) { post(C_PROTNEW, layer ? 1 : 0, 0); }
void en_set(int p, int v) { post(C_SET, p, v); }
void en_pool(uint32_t pool) { post(C_POOL, 0, (int)(pool & 0xFFFu)); }
void en_play(int on) { post(C_PLAY, on, 0); }
void en_next(void) { post(C_NEXT, 0, 0); }
void en_freeze(int on) { post(C_FREEZE, on, 0); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }
void en_rings_note(int midi_note, int velocity)
{
    /* Transposed external keys can fall below MIDI0 at the extended octaves.
     * Bound them before post() packs the pitch into an unsigned byte.
     */
    if (midi_note < 0) midi_note = 0;
    if (midi_note > 127) midi_note = 127;
    post(C_RINGNOTE, midi_note, velocity);
}
void en_fm_note(int midi_note, int velocity) { post(velocity > 0 ? C_FMNOTE : C_FMOFF, midi_note, velocity); }
void en_fm_note_off(int midi_note) { post(C_FMOFF, midi_note, 0); }
void en_fm_all_notes_off(void) { post(C_FMALLOFF, 0, 0); }

/* -------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_frozen, en_ext;
volatile uint8_t en_chord[EN_NPAD], en_chord_root, en_chord_n;
volatile float en_drift, en_pad_level, en_bass_level, en_drone_level, en_reverb_level;
volatile float en_fm_level;
volatile uint8_t en_loop_pos[EN_NLOOP];
volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];
volatile float en_loop_glow[EN_NLOOP];
volatile uint8_t en_drum_pos[EN_NDRUM];
volatile uint32_t en_drum_pat[EN_NDRUM];
volatile float en_drum_glow[EN_NDRUM];
volatile uint32_t en_bars;
volatile float en_arc;
volatile uint8_t en_pulse_rest;
volatile uint8_t en_pad_note[EN_NPAD];
volatile float en_pad_amp[EN_NPAD];

/* par defined before parameter display, which reflects filter Type compatibility. */
static uint32_t pool = 0xAADu;                  /* Ionian until told */
static int nsteps, steps[12];                   /* the scale */
static int deg;                                 /* the chord's scale degree */
static int chord_pcs[4], chord_nt;
static float tunefac = 1.0f;
static uint32_t rng = 0x9E3779B9u;
static uint32_t pad2_rng = 0x6D2B79F5u;          /* independent Pad 2 random stream */

static uint32_t rnd(void)                       /* Marsaglia's xorshift (Xorshift RNGs, 2003): shifts 13, 17, 5 */
{
    uint32_t x = rng;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    rng = x ^ (x << 5);
    return rng;
}
static float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static int irand(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }
static uint32_t pad2_rnd(void)
{
    uint32_t x = pad2_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    pad2_rng = x ? x : 1u;
    return pad2_rng;
}
static float pad2_frand(void) { return (float)(pad2_rnd() >> 8) * (1.0f / 16777216.0f); }

static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (par[P_MIDI])
        en_midi_out(st, d1, d2);
}

static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }

/* a sine table: 1024 points a turn, plus one */
#define SN 1024
static float sint[SN + 1];
static inline float sine(float ph)              /* ph in turns, any value >= 0 */
{
    float x = (ph - (float)(int)ph) * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return sint[i] + (sint[i + 1] - sint[i]) * f;
}

/* ------------------------------------------------- slow random motion --- */
#define NMOD 11                                  /* 0..5 the pads', 6..8 the echo's */
#define NMOD_PAD 6
/* Display-only waveform history; keep normal RAM reserved for time-critical stacks. */
volatile int8_t en_mod_trace[11][EN_MOD_TRACE] EN_POOL;
volatile uint16_t en_mod_trace_head, en_mod_trace_count;
static uint32_t mod_trace_samples;
static struct {
    float v, tgt, rate;
    uint32_t left;                              /* samples until a new target */
} mod[NMOD];
static struct {
    float v, tgt, rate;
    uint32_t left;
} pad2_mod[NMOD_PAD];

static int mod_depth_param(int i)
{
    static const uint8_t P[6] = {P_M1DEPTH,P_M2DEPTH,P_M3DEPTH,P_M4DEPTH,P_M5DEPTH,P_M6DEPTH};
    return i < 6 ? P[i] : i == 6 ? P_M7DEPTH : i == 7 ? P_M8DEPTH : i==8 ? P_M9DEPTH : i==9 ? P_M10DEPTH : P_M11DEPTH;
}
static int mod_speed_param(int i)
{
    static const uint8_t P[6] = {P_M1SPEED,P_M2SPEED,P_M3SPEED,P_M4SPEED,P_M5SPEED,P_M6SPEED};
    return i < 6 ? P[i] : i == 6 ? P_M7SPEED : i == 7 ? P_M8SPEED : i==8 ? P_M9SPEED : i==9 ? P_M10SPEED : P_M11SPEED;
}
static float mod_value(int i)
{
    return mod[i].v * (float)par[mod_depth_param(i)] * 0.01f;
}
static float pad2_mod_value(int i)
{
    return pad2_mod[i].v * (float)par[mod_depth_param(i)] * 0.01f;
}
/* M7..M11's selectable waveforms; 0 preserves M7..M9's legacy
 * independent smooth-random targets, 1 is sample-and-hold stepped random.
 * Sine/triangle/pulse/saw are block-rate control LFOs, not audio oscillators. */
/* Dedicated RNG keeps new FM modulation from perturbing the older pads,
 * Rings, chords and sequencer randomness. */
static uint32_t fm_mod_rng=0x38c62157u;
static float fm_mod_frand(void) {
    uint32_t x=fm_mod_rng; x^=x<<13;x^=x>>17;x^=x<<5;
    fm_mod_rng=x?x:1u;
    return (float)(fm_mod_rng>>8)*(1.0f/16777216.0f);
}
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
            mod[i].tgt=(i>=9?fm_mod_frand():frand())*2.0f-1.0f;
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

static void pad2_mods_step(uint32_t n)
{
    int i;
    for (i = 0; i < NMOD_PAD; i++) {
        if (pad2_mod[i].left <= n) {
            float sp = (float)par[mod_speed_param(i)] * 0.01f;
            float secs = 2.0f * 1200.0f * fm_exp2f(-6.321928f * sp); /* Pad 2 = half speed */
            secs *= 0.75f + 0.5f * pad2_frand();
            pad2_mod[i].tgt = pad2_frand() * 2.0f - 1.0f;
            pad2_mod[i].left = (uint32_t)(secs * EN_SR);
            pad2_mod[i].rate = (float)n / (secs * EN_SR * 0.55f);
        } else {
            pad2_mod[i].left -= n;
        }
        pad2_mod[i].v += (pad2_mod[i].tgt - pad2_mod[i].v) * fm_minf(pad2_mod[i].rate, 1.0f);
    }
}

static float motion;                            /* 0..1: Motion */
static float mod_pad2_bright, mod_pad2_body, mod_pad_body, mod_pad_char, mod_rev_size, mod_rev_damp;

static float route_mod(int dest)
{
    float sum = 0.0f;
    const int dp[3] = {P_M7DEST, P_M8DEST, P_M9DEST};
    const int ap[3] = {P_M7DEPTH, P_M8DEPTH, P_M9DEPTH};
    int k;
    for (k = 0; k < 3; k++) if (par[dp[k]] == dest)
        sum += mod[6+k].v * (float)par[ap[k]] * 0.01f;
    return sum;
}


/* ---------------------------------------------------------------- pad --- */
/* Breathing voices, as the sung tape loops of Music for Airports' "2/1": each voice swells in, hangs
 * and fades on its own cycle (unequal lengths, set by Swell), taking its note of the chord as it
 * starts, so the chord forms and dissolves rather than sitting there. The timbre: two saws a few
 * cents apart through two formant band-passes, a vowel that Bright moves from "oo" through "ah" to
 * "eh", with a little of the first formant's low-pass for body. */
typedef struct {
    float ph[2], inc[2];
    float note, target;                         /* MIDI: the note of this breath, the next one's */
    float t, rate, peak, amp;                   /* t 0..1 through the breath, per 32 samples */
    float ic[4];                                /* the two filters' states */
    float c1[4], c2[4];                         /* their coefficients: a1, a2, a3, k */
    float vib, pan;
    float sub_lp, sub_ph;                     /* octave-down Low Body filter + phase */
    float hf;                                   /* a handoff's fade: 1 normal */
    uint8_t midi_note, hand;                    /* hand: 1 fading out of the old key, 2 into the new */
    int8_t seq_shift;
    uint8_t emitted_note;
    uint8_t voices;                             /* this breath's colour: 0 Rhodes, 1 Vintage Analog */
} pad_t;
static pad_t pad[EN_NPAD];
static fm_filter_state_t pad_filters[2][2];
static float pad_motion_phase[2][4],pad_motion_wave[2][4],pad_motion_amount;
static float pad_gate, pad_gain, pad_vol, bright, swell, pad_body, pad_char;
#define PAD_CHORUS_N 1024
static int16_t pad_ch_l[PAD_CHORUS_N] EN_POOL, pad_ch_r[PAD_CHORUS_N] EN_POOL;
static uint32_t pad_ch_w;
static float pad_ch_phase;
static float pad_chorus = 0.28f;

/* Pad 2 Lite keeps the musically important independence of the second Pad
 * (four voices, its own notes/envelopes/random motion/chord clock), but removes
 * the expensive second chorus delay and per-voice resonant filters. Brightness is
 * handled by a cheap stereo bus low-pass, and the old Chorus 2 parameter becomes
 * a phase-safe stereo Width control. Audio remains full-rate at 44.1 kHz. */
static pad_t pad2[EN_NPAD];
static float pad2_rest[EN_NPAD]; /* samples until the next sparse answer */
static float pad2_gate, pad2_gain, pad2_vol, pad2_bright, pad2_swell, pad2_body, pad2_char;
static float pad2_width = 0.20f;
static float pad2_plp[4];
static int pad2_deg;
static int pad2_chord_pcs[4], pad2_chord_nt;
static float pad2_drift_left, pad2_drift_total;
static float pad2_arc, pad2_arc_tgt, pad2_arcm;
static uint32_t pad2_arc_left;

static inline int16_t pad_ch_q(float x)
{
    int32_t q = (int32_t)(fm_clampf(x, -1.0f, 1.0f) * 32767.0f);
    if (q > 32767) q = 32767;
    if (q < -32768) q = -32768;
    return (int16_t)q;
}
static float pad_rev=0.65f, ring_rev=0.75f, pad2_rev=0.45f;
static const float PAD_PAN[EN_NPAD] = {-0.55f, -0.18f, 0.18f, 0.55f};
static const float BREATH[EN_NPAD] = {1.0f, 1.21f, 1.43f, 1.67f};    /* unequal cycles, as 2/1's 12..20 s loops */
static const float START_T[EN_NPAD] = {0.0f, 0.9f, 0.82f, 0.95f};    /* the first breaths overlap */
static const float VOWEL[4][2] = {{320.0f, 800.0f}, {480.0f, 900.0f}, {730.0f, 1090.0f}, {530.0f, 1840.0f}};
static float arcm;                              /* the slow arc, -1 sparse .. +1 full, times Motion */

static float breath(float t)                    /* rise, hang, fall, a short rest */
{
    if (t < 0.4f)
        return 0.5f - 0.5f * fm_cosf(FM_PI * t * 2.5f);
    if (t < 0.55f)
        return 1.0f;
    if (t < 0.9f)
        return 0.5f + 0.5f * fm_cosf(FM_PI * (t - 0.55f) * (1.0f / 0.35f));
    return 0.0f;
}

/* Let the falling 0.55..0.90 portion advance at half rate. This doubles only
 * the audible fall, leaving the attack, hang and rest timing unchanged. */
static inline float breath_advance(float t, float rate)
{
    return t + rate * ((t >= 0.55f && t < 0.9f) ? 0.5f : 1.0f);
}

static void svf_coefs(float *c, float f, float k)
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 40.0f, 16000.0f) * (1.0f / EN_SR));
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}

static int seq_octave(int range, int chance, float roll, float direction)
{
    if (!range || chance <= 0 || roll >= (float)chance * 0.01f) return 0;
    return range == 1 || direction >= 0.5f ? 12 : -12;
}
static int seq_note(int note, int range, int chance, float roll, float direction)
{
    /* Preserve each original loop register; user drift is an additive offset. */
    return note + seq_octave(range, chance, roll, direction);
}
static void midi(uint32_t st, uint32_t d1, uint32_t d2);
static void pad_breathe(pad_t *v, int i)        /* a new breath: its length, its strength, its note */
{
    float secs = (8.0f + 0.20f * swell) * BREATH[i] * (0.85f + 0.3f * frand());
    v->rate = 32.0f / (secs * EN_SR);
    v->peak = 0.6f + 0.4f * frand();
    if (en_playing && v->midi_note)
        midi(0x80, v->emitted_note, 0);
    v->note = v->target;
    v->seq_shift = (int8_t)seq_octave(par[P_PADRANGE], par[P_PADCHANCE], frand(), frand());
    v->voices = (uint8_t)(par[P_PADTONE] != 0);
    v->midi_note = (uint8_t)v->target;
    v->emitted_note = (uint8_t)en_pad_pitch(v->midi_note, v->seq_shift, par[P_PADOCT]);
    if (en_playing)
        midi(0x90, v->emitted_note, (uint32_t)(50.0f + 40.0f * v->peak));
}

/* a new key (piece, root, scale): the voices still singing the old one fade out in about a second and
 * breathe again on the new chord; a voice at rest just takes its new note. Chord drift doesn't do
 * this: there the old notes are meant to hang over */
#define HAND_STEP (32.0f / (2.0f * EN_SR))
static void pad_handoff(void)
{
    int i;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        if (v->amp > 0.002f && ((float)v->midi_note != v->target || v->voices != (par[P_PADTONE] != 0)))
            v->hand = 1;
        else if (!v->hand)
            v->note = v->target;
    }
}

/* a chord change: a voice singing a note the new chord doesn't have fades out in about a second and
 * breathes in on its new note; a voice on a shared note holds it to the end of its breath */
static void pad_leave(void)
{
    int i, k;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        int pc = v->midi_note % 12, shared = 0;
        for (k = 0; k < chord_nt; k++)
            shared |= chord_pcs[k] == pc;
        if (v->amp > 0.002f && !shared && !v->hand)
            v->hand = 1;
        else if (v->amp <= 0.002f && !v->hand)
            v->note = v->target;
    }
}

/* pi32v2 workaround: retain call boundaries. In v17.20 the large inlined
 * render assigned r7=0 then dereferenced [r7+16] for this envelope. The device
 * reported PC 0x0200cc76 / DBG 0x10. Keep both pad control/render paths separate. */
static __attribute__((noinline)) void pad_coefs(pad_t *v, int i)          /* every 32 samples: the breath, pitch, the vowel */
{
    float f, vw, f1, f2;
    int a;
    if (v->hand == 1) {                         /* out of the old key */
        v->hf -= HAND_STEP;
        if (v->hf <= 0.0f) {
            v->hf = 0.0f;
            v->hand = 2;
            v->t = 0.15f;                       /* a breath already rising, on the new chord */
            pad_breathe(v, i);
        }
    } else if (v->hand == 2) {                  /* into the new */
        v->hf += HAND_STEP * 0.6f;
        if (v->hf >= 1.0f) {
            v->hf = 1.0f;
            v->hand = 0;
        }
    }
    v->t = breath_advance(v->t, v->rate);
    if (v->t >= 1.0f) {
        v->t -= 1.0f;
        pad_breathe(v, i);
    }
    v->vib += (4.6f + 0.4f * (float)i) * 32.0f / EN_SR;
    if (v->vib >= 1.0f)
        v->vib -= 1.0f;
    f = mtof((float)en_pad_pitch((int)v->note, v->seq_shift, par[P_PADOCT]) + 0.035f * motion * mod_value(i) + 0.04f*pad_motion_amount*pad_motion_wave[0][i]);                          /* a slow drift, as tape */
    if (v->voices) {
        float det = fm_exp2f((1.5f + 7.0f * fm_clampf(mod_pad_char, 0.0f, 1.0f)) * (1.0f / 1200.0f));
        v->inc[0] = f / det * (1.0f / EN_SR);
        v->inc[1] = f * det * (1.0f / EN_SR);
    } else {
        v->inc[0] = f * 0.99856f * (1.0f / EN_SR);  /* Rhodes stays -2.5 / +2.5 cents */
        v->inc[1] = f * 1.00144f * (1.0f / EN_SR);
    }
    if (v->voices) {
        /* Vintage Analog: a broad, warm low-pass rather than the formant bank.
         * Bright and the slow arc open it gradually; each voice differs a touch. */
        float tone = fm_clampf((float)bright * 0.01f + 0.18f * arcm + 0.10f * motion * mod_value((i + 2) % NMOD_PAD) + 0.16f*pad_motion_amount*pad_motion_wave[0][(i+1)&3], 0.0f, 1.0f);
        f1 = 420.0f * fm_exp2f(3.55f * tone) * (0.96f + 0.025f * (float)i);
        svf_coefs(v->c1, fm_clampf(f1, 320.0f, 5200.0f), 0.78f);
        svf_coefs(v->c2, 900.0f, 0.6f); /* kept initialized for seamless colour handoff */
    } else {
        vw = fm_clampf((float)bright * 0.01f + 0.25f * arcm + 0.16f * motion * mod_value((i + 2) % NMOD_PAD) + 0.16f*pad_motion_amount*pad_motion_wave[0][(i+1)&3], 0.0f, 1.0f) * 2.999f;
        a = (int)vw;
        vw -= (float)a;
        f1 = VOWEL[a][0] + (VOWEL[a + 1][0] - VOWEL[a][0]) * vw;
        f2 = VOWEL[a][1] + (VOWEL[a + 1][1] - VOWEL[a][1]) * vw;
        svf_coefs(v->c1, f1 * (1.0f + 0.03f * (float)i), 0.45f);
        svf_coefs(v->c2, f2 * (1.0f - 0.02f * (float)i), 0.3f);
    }
}

static inline float blep_saw(float t, float dt)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}


/* --------------------------------------------------------------- Pad 2 --- */
/* Pad 2 Lite does its expensive pitch/timbre control work at a much lower
 * cadence than Pad 1. The audio oscillators themselves still run every sample. */
#define PAD2_CTRL_SAMPLES 128.0f
#define PAD2_HAND_STEP (PAD2_CTRL_SAMPLES / (4.0f * EN_SR))
/* Phrase decisions run at onset, never in the sample loop.  Four allocated
 * voices remain, but answers admit at most two notes at once. */
static void pad2_end_note(pad_t *v)
{
    if(en_playing && v->midi_note)midi(0x84,v->emitted_note,0);
    v->midi_note=0;
}
static void pad2_gap(pad_t *v,int i)
{
    pad2_end_note(v);v->peak=0.0f;v->t=0.0f;
    pad2_rest[i]=(3.0f+0.04f*pad2_swell+6.0f*pad2_frand())*EN_SR;
}
static void pad2_breathe(pad_t *v, int i)
{
    int active=0,nt=pad2_chord_nt;
    float secs=(2.5f+0.055f*pad2_swell)*BREATH[i]*(0.85f+0.3f*pad2_frand());
    pad2_end_note(v);
    for(int j=0;j<EN_NPAD;j++)if(j!=i && pad2[j].peak>0.0f && pad2_rest[j]<=0.0f)active++;
    if(active>=2 || !nt || pad2_frand()>0.65f){pad2_gap(v,i);return;}
    /* Follow the foundation's chord, favour its upper tones over the root. */
    int idx=nt>1 && pad2_frand()<0.8f?1+(int)(pad2_rnd()%(unsigned)(nt-1)):0;
    int shift=12+seq_octave(par[P_PAD2RANGE],par[P_PAD2CHANCE],pad2_frand(),pad2_frand());
    int note=0;
    for(int tries=0;tries<nt;tries++){
        int pc=pad2_chord_pcs[(idx+tries)%nt],used=0;
        note=en_pad_pitch(pc,shift,par[P_PAD2OCT]);
        for(int j=0;j<EN_NPAD;j++)if(j!=i && pad2[j].midi_note && pad2[j].emitted_note==note)used=1;
        if(!used)break;
        if(tries==nt-1){pad2_gap(v,i);return;}
    }
    pad2_rest[i]=0.0f;
    v->rate=PAD2_CTRL_SAMPLES/(secs*EN_SR);
    v->peak=0.45f+0.30f*pad2_frand();
    v->note=(float)(note%12+60);
    v->seq_shift=(int8_t)shift;
    v->voices=(uint8_t)(par[P_DRONEVOICE]!=0);
    v->midi_note=(uint8_t)v->note;
    v->emitted_note=(uint8_t)note;
    if(en_playing)midi(0x94,v->emitted_note,(uint32_t)(50.0f+40.0f*v->peak));
}

static void pad2_handoff(void)
{
    int i;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad2[i];
        if (v->amp > 0.002f && ((float)v->midi_note != v->target || v->voices != (par[P_DRONEVOICE] != 0)))
            v->hand = 1;
        else if (!v->hand)
            v->note = v->target;
    }
}

static void pad2_leave(void)
{
    int i, k;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad2[i];
        int pc = v->midi_note % 12, shared = 0;
        for (k = 0; k < pad2_chord_nt; k++)
            shared |= pad2_chord_pcs[k] == pc;
        if (v->amp > 0.002f && !shared && !v->hand)
            v->hand = 1;
        else if (v->amp <= 0.002f && !v->hand)
            v->note = v->target;
    }
}

/* Low-cost Pad 2 control update. The original v17.3 duplicate recalculated two
 * TPT/SVF coefficient sets per voice every 32 samples. Lite has no per-voice
 * resonant filter, so this only updates the half-speed envelope state and pitch. */
static __attribute__((noinline)) void pad2_coefs_lite(pad_t *v, int i)
{
    float f;
    if(pad2_rest[i]>0.0f){
        pad2_rest[i]-=PAD2_CTRL_SAMPLES;
        if(pad2_rest[i]>0.0f)return;
        v->t=0.0f;pad2_breathe(v,i);
        if(pad2_rest[i]>0.0f)return;
    }
    if (v->hand == 1) {
        v->hf -= PAD2_HAND_STEP;
        if (v->hf <= 0.0f) {
            v->hf = 0.0f;
            v->hand = 2;
            v->t = 0.15f;
            pad2_breathe(v, i);
        }
    } else if (v->hand == 2) {
        v->hf += PAD2_HAND_STEP * 0.6f;
        if (v->hf >= 1.0f) { v->hf = 1.0f; v->hand = 0; }
    }
    v->t = breath_advance(v->t, v->rate);
    if (v->t >= 1.0f) { pad2_gap(v,i);return; }

    f = mtof((float)en_pad_pitch((int)v->note, v->seq_shift, par[P_PAD2OCT]) + 0.035f * motion * pad2_mod_value(i) + 0.06f*pad_motion_amount*pad_motion_wave[1][i]);
    {
        /* Character still changes the detune/spread of the two oscillators, but
         * uses one exp2 at control rate instead of filter/timbre math per sample. */
        float ch = fm_clampf(pad2_char, 0.0f, 1.0f);
        float cents = v->voices ? (4.0f + 8.0f * ch) : (1.5f + 3.0f * ch);
        float det = fm_exp2f(cents * (1.0f / 1200.0f));
        v->inc[0] = f / det * (1.0f / EN_SR);
        v->inc[1] = f * det * (1.0f / EN_SR);
    }
}

static void pad2_retarget(void)
{
    int from[4], to[4], i;
    pad2_deg=deg;
    pad2_chord_nt = en_chord_tones(pool, par[P_ROOT], deg, pad2_chord_pcs);
    for (i = 0; i < EN_NPAD; i++) from[i] = (int)(pad2[i].target + 0.5f);
    en_voice_lead(from, pad2_chord_pcs, pad2_chord_nt, to, 43, 72);
    for (i = 0; i < EN_NPAD; i++) pad2[i].target = (float)to[i];
}

static void pad2_drift_reset(void)
{
    pad2_drift_total = 2.0f * drift_secs(par[P_DRIFT]) * (0.7f + 0.6f * pad2_frand()) * EN_SR;
    pad2_drift_left = pad2_drift_total;
}

static void pad2_chord_change(void)
{
    /* Revisit the current foundation instead of starting a second progression. */
    pad2_retarget();pad2_leave();pad2_drift_reset();
}

static void pad2_control_clock(uint32_t n)
{
    if (pad2_arc_left <= n) {
        float r = pad2_frand() * 2.0f - 1.0f;
        pad2_arc_tgt = r * (0.6f + 0.4f * pad2_frand()) + (r > 0.0f ? 0.25f : -0.25f);
        pad2_arc_left = (uint32_t)((480.0f + 720.0f * pad2_frand()) * EN_SR);
    } else pad2_arc_left -= n;
    if (en_playing)
        pad2_arc += (fm_clampf(pad2_arc_tgt, -1.0f, 1.0f) - pad2_arc) * (float)n * (1.0f / (180.0f * EN_SR));
    pad2_arcm = pad2_arc * motion;
    if (en_playing && !en_frozen) {
        pad2_drift_left -= (float)n;
        if (pad2_drift_left <= 0.0f) pad2_chord_change();
    }
}

/* Independent, low-rate movement for the two Pad layers. Polynomial-smoothed
 * triangles avoid per-sample oscillators, libm calls and shared RNG changes.
 * Four unequal cycles per layer drive different voices/axes. */

static float pad_motion_curve(float phase)
{
    float x=phase<0.5f?4.0f*phase-1.0f:3.0f-4.0f*phase;
    return x*(1.5f-0.5f*x*x);
}
static void pad_movement_init(void)
{
    for(int l=0;l<2;l++)for(int i=0;i<4;i++){
        pad_motion_phase[l][i]=0.11f+(float)i*0.19f+(float)l*0.07f;
        pad_motion_wave[l][i]=pad_motion_curve(pad_motion_phase[l][i]);
    }
    pad_motion_amount=0.0f;
}
static void pad_movement_step(uint32_t n)
{
    static const float hz[2][4]={{1.0f/13.0f,1.0f/19.0f,1.0f/29.0f,1.0f/37.0f},
                                  {1.0f/8.0f,1.0f/11.0f,1.0f/17.0f,1.0f/23.0f}};
    float dt=(float)n*(1.0f/EN_SR);
    pad_motion_amount+=(motion-pad_motion_amount)*fm_minf(dt*(1.0f/0.3f),1.0f);
    if(motion==0.0f && pad_motion_amount<0.00001f)pad_motion_amount=0.0f;
    if(!en_playing)return;
    for(int l=0;l<2;l++)for(int i=0;i<4;i++){
        float p=pad_motion_phase[l][i]+dt*hz[l][i];
        if(p>=1.0f)p-=1.0f;
        pad_motion_phase[l][i]=p;pad_motion_wave[l][i]=pad_motion_curve(p);
    }
}

/* ---------------------------------------------------------- legacy bass slots --- */
/* P_BASS/P_BASSOCT/P_GLIDE/P_FIFTH/P_BASSREV remain in the saved-project
 * parameter layout for backward compatibility, but the Bass engine itself is removed. */

/* ----------------------------------------------------------------- bells --- */
#define NBELL 6
typedef struct {
    float cph, mph, cinc, minc, amp, adec, idx, idec, att, gl, gr;
    int32_t wait;
    uint8_t on, note, loop;
} bell_t;
static bell_t bell[NBELL];
static float bell_vol, btone;
static int8_t loop_note[EN_NLOOP][EN_LOOP_MAX];
static uint8_t loop_vel[EN_NLOOP][EN_LOOP_MAX], loop_pos[EN_NLOOP];
static const uint8_t LOOP_BASE[EN_NLOOP] = {72, 67, 79, 62};     /* each loop's register */
static const float LOOP_PAN[EN_NLOOP] = {-0.6f, 0.35f, 0.7f, -0.25f};
static const float RATIO[5] = {1.0f, 2.0f, 3.0f, 3.5f, 4.77f};

/* ----------------------------------------------------------------- pulse --- */
typedef struct {
    float ph, inc, inc_end, amp, adec, idx, x1, x2, att, mph[4], pend;
    int32_t wait;                               /* swing: samples until a hit that waits (pend: its velocity) */
    uint8_t on, note;
} drum_t;
static drum_t drum[EN_NDRUM];
static uint8_t drum_pos[EN_NDRUM];
static uint32_t drum_pat[EN_NDRUM];
static float pulse_vol, swing, hat_f = 0.9f, hat_open = 0.42f;
static float step_len, pulse_left;               /* a sixteenth, in samples; to the next MIDI clock pulse */
static uint32_t noise = 22222u;
static const uint8_t DRUM_KMAX[EN_NDRUM] = {5, 7, 3};
static const uint8_t DRUM_GM[EN_NDRUM] = {36, 42, 46};          /* kick, closed hat, open hat */
/* the hats' metal: four square waves at the 808's inharmonic ratios (Hz / EN_SR) */
static const float HAT_INC[4] = {304.4f / EN_SR, 369.6f / EN_SR, 522.7f / EN_SR, 800.0f / EN_SR};

/* ------------------------------------------------------------------ space --- */
static float echo_send, rev_send, echo_len, echo_len_t, echo_fb = 0.55f;
static float rev_in_gain = 1.0f;                /* falls to 0 while frozen */

/* -------------------------------------------------------------- harmony --- */
static void scale_set(void)
{
    nsteps = en_scale(pool, steps);
}

static int bell_note(int l, int off)            /* a loop slot's note, from the chord's degree */
{
    int d = deg + off, oct = d / nsteps, s = steps[d % nsteps], base, root = par[P_ROOT], n;
    base = LOOP_BASE[l] - ((LOOP_BASE[l] - root) % 12 + 12) % 12;   /* the root at or below the register */
    n = base + s + 12 * oct;
    return en_original_octave_pitch(n, par[P_BOCT]);
}

static void pad_retarget(void)                  /* the chord's tones to the four voices */
{
    int from[4], to[4], i;
    chord_nt = en_chord_tones(pool, par[P_ROOT], deg, chord_pcs);
    for (i = 0; i < EN_NPAD; i++)
        from[i] = (int)(pad[i].target + 0.5f);
    en_voice_lead(from, chord_pcs, chord_nt, to, 48, 77);
    for (i = 0; i < EN_NPAD; i++) {             /* each voice takes its note at its next breath */
        pad[i].target = (float)to[i];
        en_chord[i] = (uint8_t)to[i];
    }
    en_chord_root = (uint8_t)chord_pcs[0];
    en_chord_n = (uint8_t)chord_nt;
    pad2_retarget();
    pad2_leave();
}

static float drift_left, drift_total, evolve;   /* evolve: Evolve, 0..1 */
static uint8_t change_pending, fifth_done;

static void drift_reset(void)
{
    drift_total = drift_secs(par[P_DRIFT]) * (0.7f + 0.6f * frand()) * EN_SR;
    drift_left = drift_total;
    fifth_done = 0;
}

static void chord_change(void)                  /* to another degree: the near, strong moves first */
{
    static const float W7[7] = {4.0f, 2.0f, 1.0f, 3.0f, 3.0f, 3.0f, 0.4f};
    float w[12], sum = 0.0f, r;
    int i;
    for (i = 0; i < nsteps; i++) {
        w[i] = i == deg ? 0.0f : nsteps == 7 ? W7[i] : i == 0 ? 3.0f : 1.5f;
        sum += w[i];
    }
    r = frand() * sum;
    for (i = 0; i < nsteps; i++) {
        r -= w[i];
        if (r <= 0.0f && w[i] > 0.0f)
            break;
    }
    if (i >= nsteps)
        i = deg == 0 ? (nsteps > 1 ? 1 : 0) : 0;
    deg = i;
    pad_retarget();
    pad_leave();
    drift_reset();
}

/* ------------------------------------------------------------- the loops --- */
static int rand_offset(void)                    /* scale steps above the chord's degree */
{
    if (frand() < 0.72f) {                      /* a chord tone, sometimes an octave up */
        int o = 2 * irand(chord_nt);
        return o + (frand() < 0.3f ? nsteps : 0);
    }
    return 1 + 2 * irand(3);                    /* a passing note: the 2nd, 4th or 6th */
}

static int loop_count(int l)
{
    int i, c = 0;
    for (i = 0; i < EN_LOOP_LEN[l]; i++)
        c += loop_note[l][i] >= 0;
    return c;
}

static int loop_target(void)                    /* 0..5 notes: Density, swelled and thinned by the arc */
{
    float d = (float)par[P_DENSITY] * 0.05f * (1.0f + 0.8f * arcm);
    return (int)(fm_clampf(d, 0.0f, 5.0f) + frand());
}
static uint8_t loop_rest[EN_NLOOP];             /* this pass: silent (Evolve) */

static void loop_add(int l)
{
    int len = EN_LOOP_LEN[l], i, s;
    for (i = 0; i < 8; i++) {
        s = irand(len);
        if (loop_note[l][s] < 0) {
            loop_note[l][s] = (int8_t)rand_offset();
            loop_vel[l][s] = (uint8_t)(50 + irand(50));
            return;
        }
    }
}

static int loop_pick(int l)                     /* a random sounding slot, -1 none */
{
    int len = EN_LOOP_LEN[l], i, s = irand(len);
    for (i = 0; i < len; i++, s = (s + 1) % len)
        if (loop_note[l][s] >= 0)
            return s;
    return -1;
}

static void loop_publish(int l)
{
    int i;
    for (i = 0; i < EN_LOOP_MAX; i++)
        en_loop_note[l][i] = i < EN_LOOP_LEN[l] ? loop_note[l][i] : -1;
}

static void loop_regrow(int l)
{
    int i, n = loop_target();
    for (i = 0; i < EN_LOOP_MAX; i++)
        loop_note[l][i] = -1;
    for (i = 0; i < n; i++)
        loop_add(l);
    loop_publish(l);
}

static void loop_pass(int l)                    /* the end of a pass: grow, prune, evolve */
{
    int c = loop_count(l), t = loop_target(), s;
    for (; c < t; c++)                          /* Density: to its count by the end of a pass */
        loop_add(l);
    for (; c > t && (s = loop_pick(l)) >= 0; c--)
        loop_note[l][s] = -1;
    loop_rest[l] = frand() < evolve * 0.15f + 0.15f * fm_maxf(-arcm, 0.0f);
    if (frand() < evolve * 0.7f && (s = loop_pick(l)) >= 0) {
        if (frand() < 0.5f) {
            loop_note[l][s] = (int8_t)rand_offset();             /* another note */
        } else {
            int d = irand(EN_LOOP_LEN[l]);                       /* the same note, elsewhere */
            if (loop_note[l][d] < 0) {
                loop_note[l][d] = loop_note[l][s];
                loop_vel[l][d] = loop_vel[l][s];
                loop_note[l][s] = -1;
            }
        }
    }
    loop_publish(l);
}

/* each loop its own bell: index, decay and ratio a little apart, so the four can be told apart */
static const float LOOP_IDX[EN_NLOOP] = {1.0f, 0.55f, 1.5f, 0.8f}, LOOP_DEC[EN_NLOOP] = {1.0f, 1.35f, 0.75f, 1.6f};
static const int8_t LOOP_RAT[EN_NLOOP] = {0, 0, 1, -1};

static void bell_voice(int l, int n, float vel, int32_t wait)
{
    int i, best = 0, ri;
    float lo = 9.0f, t60, p;
    bell_t *v;
    for (i = 0; i < NBELL; i++) {               /* a free voice, or the quietest */
        float a = bell[i].on ? bell[i].amp : -1.0f;
        if (a < lo) {
            lo = a;
            best = i;
        }
    }
    v = &bell[best];
    if (v->on)
        midi(0x81, v->note, 0);
    v->on = 1;
    v->note = (uint8_t)n;
    v->loop = (uint8_t)l;
    v->cph = v->mph = 0.0f;
    v->cinc = mtof((float)n) * (1.0f / EN_SR);
    ri = par[P_BTONE] * 5 / 101 + LOOP_RAT[l];
    v->minc = v->cinc * RATIO[ri < 0 ? 0 : ri > 4 ? 4 : ri];
    v->amp = vel;
    t60 = 4.2f - (float)(n - 60) * 0.06f;       /* the low bells ring longer */
    t60 = fm_clampf(t60, 1.6f, 5.0f) * LOOP_DEC[l];
    v->adec = fm_expf(-6.9f / (t60 * EN_SR));
    /* Bell tone: 0 a round, nearly pure tone; 100 a long metallic clang (index in radians, as turns) */
    v->idx = (0.15f + 6.5f * btone * btone) * LOOP_IDX[l] * (0.6f + 0.4f * vel) * (1.0f / FM_TWO_PI);
    v->idec = fm_expf(-6.9f / ((0.35f + 2.6f * btone) * EN_SR));
    v->att = 0.0f;
    p = (LOOP_PAN[l] + 1.0f) * (FM_PI / 4.0f);
    v->gl = fm_cosf(p);
    v->gr = fm_sinf(p);
    v->wait = wait;
    en_loop_glow[l] = 1.0f;
    midi(0x91, (uint32_t)n, (uint32_t)(vel * 110.0f));
}

/* Rings strikes are deliberately humanized exactly like fm1-amb's bell layer.
 * This is not just cosmetic: rings_port_trigger() has one pending strike slot, so
 * four loop notes fired on the same quarter-note tick could otherwise overwrite
 * one another before the next audio render. Small independent delays make every
 * density hit reach Rings as its own excitation while preserving the loose AMB feel. */
#define NRING_WAIT 32
typedef struct {
    int32_t wait;
    uint8_t note, velocity, active;
} ring_wait_t;
static ring_wait_t ring_wait[NRING_WAIT];

static void ring_schedule(int note, int velocity, int32_t wait)
{
    int i, slot = -1;
    if (note < 0) note = 0;
    if (note > 127) note = 127;
    if (velocity < 1) velocity = 1;
    if (velocity > 127) velocity = 127;
    if (wait < 0) wait = 0;
    /* Avoid the very rare identical-delay collision: one sample is enough for
     * Rings to consume the first pending strum before the next is posted. */
    for (;;) {
        int clash = 0;
        for (i = 0; i < NRING_WAIT; i++)
            if (ring_wait[i].active && ring_wait[i].wait == wait) { clash = 1; break; }
        if (!clash) break;
        wait++;
    }
    for (i = 0; i < NRING_WAIT; i++)
        if (!ring_wait[i].active) { slot = i; break; }
    if (slot < 0) {
        /* This should be unreachable at normal 30..120 bpm: the longest wait is
         * < 4.6k samples and a quarter note is >= 5.5k samples. If an external
         * clock is extreme, replace the latest event rather than losing the
         * nearer musical strike. */
        int32_t latest = -1;
        for (i = 0; i < NRING_WAIT; i++)
            if (ring_wait[i].wait > latest) { latest = ring_wait[i].wait; slot = i; }
        if (latest <= wait) return;
    }
    ring_wait[slot].wait = wait;
    ring_wait[slot].note = (uint8_t)note;
    ring_wait[slot].velocity = (uint8_t)velocity;
    ring_wait[slot].active = 1;
}

static int32_t ring_rate_wait = -1;
static void ring_rate_fire_due(void);
static void rings_fire_due(void)
{
    ring_rate_fire_due();
    int i;
    for (i = 0; i < NRING_WAIT; i++)
        if (ring_wait[i].active && ring_wait[i].wait <= 0) {
            rings_port_trigger((int)ring_wait[i].note, (int)ring_wait[i].velocity);
            ring_wait[i].active = 0;
        }
}

static int32_t rings_wait_next(void)
{
    int i;
    int32_t m = ring_rate_wait > 0 ? ring_rate_wait : 0;
    for (i = 0; i < NRING_WAIT; i++)
        if (ring_wait[i].active && ring_wait[i].wait > 0 && (!m || ring_wait[i].wait < m))
            m = ring_wait[i].wait;
    return m;
}

static void rings_wait_step(int32_t n)
{
    if(ring_rate_wait>=0)ring_rate_wait-=n;
    int i;
    for (i = 0; i < NRING_WAIT; i++)
        if (ring_wait[i].active)
            ring_wait[i].wait -= n;
}

static void rings_wait_clear(void)
{
    ring_rate_wait=-1;
    int i;
    for (i = 0; i < NRING_WAIT; i++)
        ring_wait[i].active = 0;
}

/* A Burst stays inside the existing generative harmony: when a scheduled loop note
 * wins the probability test, its original strike is followed by 1..3 tones from
 * the current chord. The tones are ordered up or down and spread over a short,
 * human strum. PIANO mode never calls bell_ring(), so Burst is generative-only. */
static int burst_tone(int from, int pc, int up)
{
    int n = from;
    if (up) {
        do { n++; } while (n <= 103 && ((n % 12) + 12) % 12 != pc);
        if (n > 103) n = from; /* no room above: caller can stop */
    } else {
        do { n--; } while (n >= 36 && ((n % 12) + 12) % 12 != pc);
        if (n < 36) n = from;
    }
    return n;
}

static void ring_sequence_strike(int n, float vel, int l)
{
    int base_vel = (int)(vel * 110.0f);
    int32_t wait = irand(2600);                  /* fm1-amb: a human hand, up to ~60 ms late */
    ring_schedule(n, base_vel, wait);
    en_loop_glow[l] = 1.0f;
    midi(0x91, (uint32_t)n, (uint32_t)base_vel);
    midi(0x81, (uint32_t)n, 0);

    if (par[P_RBURST] > 0 && chord_nt > 0 && frand() < (float)par[P_RBURST] * 0.01f) {
        int extra = 1 + irand(3);                /* 2..4 notes total, including the original */
        int start = irand(chord_nt);
        int last = n;
        int32_t w = wait;
        int j;
        for (j = 0; j < extra; j++) {
            int pc = chord_pcs[(start + j) % chord_nt];
            int nn = (n / 12) * 12 + pc; /* strum remains in this event's selected octave */
            int vv;
            while(nn>127)nn-=12;
            if (nn == last) break;
            /* About 35..105 ms between fingers (three times the previous spacing); ring_schedule also prevents
             * same-sample collisions with notes from the other loop clocks. */
            w += 3 * (520 + irand(1024));
            vv = base_vel - 12 - j * 8 + irand(7);
            if (vv < 25) vv = 25;
            ring_schedule(nn, vv, w);
            midi(0x91, (uint32_t)nn, (uint32_t)vv);
            midi(0x81, (uint32_t)nn, 0);
            last = nn;
        }
    }


}

static void bell_ring(int l, int slot)
{
    int n = seq_note(bell_note(l, loop_note[l][slot]), par[P_RINGRANGE], par[P_RINGCHANCE], frand(), frand());
    ring_sequence_strike(n, (float)loop_vel[l][slot] * 0.01f, l);
}

/* Sequencer calls are forward-declared because both synth schedulers are shared. */
static void prot_init(void);
static void prot_reset_position(int layer);
static void prot_tick(int layer);
static void prot_new(int layer);
static uint32_t prot_gate_samples(void);
#include "evolve.c"
static uint32_t ud_gate_samples(void);
static void ud_tick(int layer);
static void ud_reset(int layer);
static void ud_init(void);
#include "fm_sequence.c"
#include "proteus.c"
#include "unitd.c"
static void ring_rate_fire_due(void) {
    if(ring_rate_wait!=0)return;
    ring_rate_wait=-1;
    if(!en_playing || par[P_MODE]==MODE_PIANO)return;
    if(en_ring_seq_active_mode==3)ud_tick(0);
    else if(en_ring_seq_active_mode==2)prot_tick(0);
    else if(en_evo_active)evo_tick();
}

/* ---------------------------------------------------------------- pulse --- */
static const uint8_t DRUM_ROT[EN_NDRUM] = {0, 2, 3};
static int drum_k[EN_NDRUM], drum_kb[EN_NDRUM], drum_rot[EN_NDRUM], pulse_rest;   /* rest: bars left */

static void drum_publish(int d)
{
    drum_pat[d] = en_euclid(drum_k[d], EN_DRUM_N[d], drum_rot[d]);
    en_drum_pat[d] = drum_pat[d];
}

static void drums_pattern(void)                 /* Hits: the patterns' home */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++) {
        drum_kb[d] = drum_k[d] = (par[P_HITS] * DRUM_KMAX[d] + 50) / 100;
        drum_rot[d] = DRUM_ROT[d];
        drum_publish(d);
    }
}

static void drums_evolve(void)                  /* a bar: a pattern turns, or gains or loses a hit */
{
    int d = irand(EN_NDRUM), n = EN_DRUM_N[d];
    if (!drum_kb[d])
        return;
    if (frand() < 0.5f) {
        drum_rot[d] = (drum_rot[d] + (frand() < 0.5f ? 1 : n - 1)) % n;
    } else {
        int k = drum_k[d] + (frand() < 0.5f ? 1 : -1);
        if (k >= drum_kb[d] - 1 && k <= drum_kb[d] + 1 && k >= 1 && k <= n)
            drum_k[d] = k;
    }
    drum_publish(d);
}

static void drum_fire(int d, float vel)
{
    drum_t *v = &drum[d];
    v->on = 1;
    v->amp = vel;
    v->att = 0.0f;
    switch (d) {
    case 0: {                                   /* thump: the chord's root, low, falling into place */
        float f = mtof((float)(36 + chord_pcs[0]));
        v->note = DRUM_GM[0];
        v->inc = f * 2.2f * (1.0f / EN_SR);
        v->inc_end = f * (1.0f / EN_SR);
        v->adec = fm_expf(-6.9f / (0.45f * EN_SR));
        break;
    }
    case 1:                                     /* closed hat: short; it chokes the open one */
        v->note = DRUM_GM[1];
        v->adec = fm_expf(-6.9f / (0.06f * EN_SR));
        drum[2].adec = fm_expf(-6.9f / (0.02f * EN_SR));
        break;
    default:                                    /* open hat: a long, soft "tss" */
        v->note = DRUM_GM[2];
        v->adec = fm_expf(-6.9f / (hat_open * EN_SR));
        break;
    }
    en_drum_glow[d] = 1.0f;
    midi(0x99, v->note, (uint32_t)(vel * 100.0f));
    midi(0x89, v->note, 0);
}

static void drum_hit(int d, float vel, int off)  /* off: an off sixteenth, which Swing delays */
{
    int32_t w = off ? (int32_t)(swing * step_len) : 0;
    if (w > 0) {
        drum[d].wait = w;
        drum[d].pend = vel;
    } else {
        drum_fire(d, vel);
    }
}

static int32_t drums_wait(void)                 /* samples to the next hit that waits; 0: none */
{
    int32_t m = 0;
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (!m || drum[d].wait < m))
            m = drum[d].wait;
    return m;
}

static void drums_wait_step(int32_t k)          /* k samples went by: hits whose wait is over */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (drum[d].wait -= k) <= 0) {
            drum[d].wait = 0;
            drum_fire(d, drum[d].pend);
        }
}

/* the grid: one call a sixteenth (MIDI clock: 6 ticks) */
static uint32_t sixteenth;
static void tick(void)
{
    int l;
    if ((sixteenth & 15u) == 0u) evo_select();
    if (en_evo_active && (sixteenth & 15u) == 0u && change_pending && !en_frozen) {
        change_pending=0; chord_change();
    }
    ring_rate_wait=-1;
    if(par[P_MODE]!=MODE_PIANO && en_ring_seq_active_mode!=0) {
        int p=en_ring_seq_active_mode==3?P_RUDRATE:en_ring_seq_active_mode==2?P_RPROTRATE:P_ERATE;
        float len=en_ext?fm_quarter_samples*0.25f:step_len;
        ring_rate_wait=seq_rate_offset(par[p],sixteenth,len);
        ring_rate_fire_due();
    }
    if (en_ring_seq_active_mode == 0 && (sixteenth & 3u) == 0u) {
        for (l = 0; l < EN_NLOOP; l++) {
            int ss = loop_pos[l];
            if (par[P_MODE] != MODE_PIANO && loop_note[l][ss] >= 0 && !loop_rest[l]) bell_ring(l, ss);
            en_loop_pos[l] = (uint8_t)ss;
            if (++loop_pos[l] >= EN_LOOP_LEN[l]) { loop_pos[l] = 0; loop_pass(l); }
        }
    }
    if ((sixteenth & 15u) == 0u && change_pending && !en_frozen) {
        change_pending = 0;
        chord_change();
    }
    fm_seq_tick(sixteenth);
    sixteenth++;
    en_bars = sixteenth;
}

#if 1 /* Monumatic TEST1 replacement; full-rate recovered eight-line tank. */
static float pl_mod_depth = 1.0f;
static void plate_init(void){monumatic_reverb_init();}

#if 0 /* long tape/Echoplex intentionally remain removed. */
/* ------------------------------------------------- echo: tape and Echoplex --- */
/* After Discreet Music's chain: an Echoplex (a short, smearing echo, ~160 ms, fed back) and a tape loop
 * (long: up to ~5.6 s, synced to the tempo), each repeat a little darker and thinner, with a little
 * wow. The tape runs at a quarter rate (11 kHz, 16-bit, mono): it loses its highs anyway. Two heads:
 * the first plays the loop's length, the second a part of it (3/4 at first). With Motion the echo
 * evolves: each head wanders across the field, the feedback leans now on one head, now on the other
 * (the repeats fall in changing patterns, left and right), and now and then the second head slides to
 * another part of the loop, as a head moved by hand (the repeats bend a little in pitch on the way).
 * The Echoplex has a second tap, for the right. */
#define TAPE_N 62000                             /* 5.6 s at 11.025 kHz */
#define TAPE_SR 11025.0f
static int16_t tape[TAPE_N] EN_POOL;
static int tape_w, tape_ph;
static float tape_acc, tape_lp, tape_hp, tape_p1, tape_p2, tape_c1, tape_c2;
static const float HEAD2[5] = {0.75f, 0.6667f, 0.625f, 0.5f, 0.375f};   /* the second head's parts */
static float head2 = 0.75f, head2_t = 0.75f, head2_v;   /* its place (of the loop), target, speed */
static float fb_lean;                            /* 0..1: how much of the feedback is the second head's */
static float hg[4] = {1.0f, 0.0f, 0.0f, 1.0f};   /* the heads' pan gains: 1 left, 1 right, 2 left, 2 right */
static uint32_t head_left;                       /* samples until the second head may move */
#define PLEX_N 7100                              /* 160 ms at 44.1 kHz */
#define PLEX_TAP 2700                            /* the right's tap: ~100 ms */
static int16_t plex[PLEX_N];
static int plex_w;
static float plex_lp, plex_lp2;

static inline float tape_read(float d)           /* d quarter-rate samples ago */
{
    int k = (int)d, i0, i1;
    float f = d - (float)k, a, c;
    i0 = tape_w - k;
    i1 = i0 - 1;
    if (i0 < 0)
        i0 += TAPE_N;
    if (i1 < 0)
        i1 += TAPE_N;
    a = (float)tape[i0];
    c = (float)tape[i1];
    return (a + (c - a) * f) * (1.0f / 12000.0f);
}
static inline int16_t s16(float x)
{
    x *= 12000.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}

static void echo_time(void)
{
    int i = par[P_ETIME];
    float beat, t;
    if (par[P_TEMPO] <= 0)
        return;                                  /* not yet (en_init applies in order) */
    beat = 60.0f / (float)par[P_TEMPO];
    t = ETIME_BEATS[i] * beat;
    while (t * TAPE_SR > (float)(TAPE_N - 8) && i > 0)
        t = ETIME_BEATS[--i] * beat;             /* too long for the tape: the next shorter */
    if (t * TAPE_SR > (float)(TAPE_N - 8))
        t = (float)(TAPE_N - 8) / TAPE_SR;
    echo_len_t = t * TAPE_SR;
    if (echo_len <= 0.0f)
        echo_len = echo_len_t;
}

/* once a chunk: where the heads sit in the field, how the feedback leans, the second head's slide */
static void heads_step(uint32_t n)
{
    float p, want, vmax;
    int i;
    for (i = 0; i < 2; i++) {
        p = fm_clampf((i ? 0.7f : -0.7f) + 0.9f * motion * mod[6 + i].v, -1.0f, 1.0f);
        p = (p + 1.0f) * (FM_PI / 4.0f);
        hg[2 * i] = fm_cosf(p);
        hg[2 * i + 1] = fm_sinf(p);
    }
    fb_lean = 0.25f * motion * (1.0f + mod[8].v);
    if (head_left <= n) {
        head_left = (uint32_t)((20.0f + 40.0f * frand()) * EN_SR);
        if (en_playing && !en_frozen && frand() < motion)
            head2_t = HEAD2[(int)(frand() * 4.999f)];
    } else {
        head_left -= n;
    }
    /* its speed (a part of the loop per tape sample): at most a bend of ~14 cents, easing in and out */
    vmax = 0.008f / fm_maxf(echo_len, 100.0f);
    want = fm_clampf((head2_t - head2) * (1.0f / (2.0f * TAPE_SR)), -vmax, vmax);
    head2_v += (want - head2_v) * fm_minf((float)n * (1.0f / EN_SR), 1.0f);
}

/* one sample of both: the input (a mono send) to the tape's heads (el, er) and the Echoplex (xl, xr) */
static inline void echoes(float in, float *el, float *er, float *xl, float *xr)
{
    float y = (float)plex[plex_w] * (1.0f / 12000.0f);         /* the Echoplex: 160 ms, fed back, darker */
    int t = plex_w - PLEX_TAP;
    plex_lp += 0.35f * (y - plex_lp);
    plex_lp2 += 0.35f * ((float)plex[t < 0 ? t + PLEX_N : t] * (1.0f / 12000.0f) - plex_lp2);
    plex[plex_w] = s16(in + 0.55f * plex_lp);
    if (++plex_w >= PLEX_N)
        plex_w = 0;
    *xl = plex_lp;
    *xr = plex_lp2;
    tape_acc += in + 0.3f * plex_lp;
    if (++tape_ph >= 4) {                        /* the tape, a quarter of the time */
        float d, h1, h2, fb;
        tape_ph = 0;
        echo_len += (echo_len_t - echo_len) * 0.001f;
        head2 += head2_v;
        d = echo_len * (1.0f + 0.003f * motion * mod[5].v);   /* wow */
        if (d > (float)(TAPE_N - 4))
            d = (float)(TAPE_N - 4);
        h1 = tape_read(d);
        h2 = tape_read(d * head2);
        tape_lp += 0.55f * (h1 + fb_lean * (h2 - h1) - tape_lp);   /* each time round: darker, thinner */
        tape_hp += 0.03f * (tape_lp - tape_hp);
        fb = tape_lp - tape_hp;
        if (++tape_w >= TAPE_N)
            tape_w = 0;
        tape[tape_w] = s16(tape_acc * 0.25f + echo_fb * fb);
        tape_acc = 0.0f;
        tape_p1 = tape_c1;
        tape_p2 = tape_c2;
        tape_c1 = h1;
        tape_c2 = h2;
    }
    {   /* between the tape's samples: a line from the last to the next; then to the field */
        float f = (float)tape_ph * 0.25f;
        float h1 = tape_p1 + (tape_c1 - tape_p1) * f, h2 = tape_p2 + (tape_c2 - tape_p2) * f;
        *el = hg[0] * h1 + hg[2] * h2;
        *er = hg[1] * h1 + hg[3] * h2;
    }
}

#endif /* tape/Echoplex */
#endif

/* ------------------------------------------------------------ parameters --- */
static int pulse;
static uint32_t ext_age;

static void tempo_set(void) { step_len = EN_SR * 60.0f / ((float)par[P_TEMPO] * 4.0f); }

static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    if (p == P_RMODEL)
        v = rings_model_normalize(v);  /* removed Rings models use their retained counterpart */
    par[p] = (int16_t)v;
    switch (p) {
    case P_MODE:
        if (v == MODE_PIANO) { rings_wait_clear(); fm_seq_stop(); fm_port_all_notes_off(); }
        break;
    case P_FMLEVEL:
        fm_port_set_level(v);
        if (!v) { fm_seq_stop(); fm_port_all_notes_off(); }
        break;
    case P_FMPATCH: fm_port_set_patch(v); break;
    case P_FMPOLY: fm_port_set_polyphony(v); break;
    case P_FMSEQMODE: case P_FMEPATTERN:
        if (!en_playing) fm_seq_select();
        break;
    case P_PAD: pad_vol = (float)(v * v) * 0.0001f; break;
    case P_BELLS: bell_vol = (float)(v * v) * 0.0001f; rings_port_set_level(v); break;
    case P_BASS: break;  /* legacy saved-project slot: Bass engine removed */
    case P_PULSE: pad2_vol = (float)(v * v) * 0.0001f; break;
    case P_DENSITY:
        /* Match fm1-amb: the knob only changes the target. Each independent
         * 13/17/23/29-step Rings loop grows or prunes toward it when that loop
         * reaches its own boundary (loop_pass), rather than being rewritten
         * immediately while the encoder is moving. */
        break;
    case P_HITS: pad2_swell = (float)v; break;
    case P_BASSOCT: break;  /* legacy */
    case P_GLIDE: break;  /* legacy */
    case P_SWING: pad2_bright = (float)v * 0.01f; break;
    case P_HATS: pad2_body = (float)v * 0.01f; break;
    case P_DRIFT:
        if (drift_total > 0.0f) {                /* keep both independent clocks at their progress */
            float frac = drift_left / drift_total;
            drift_total = drift_secs(v) * EN_SR;
            drift_left = frac * drift_total;
        }
        if (pad2_drift_total > 0.0f) {
            float frac2 = pad2_drift_left / pad2_drift_total;
            pad2_drift_total = 2.0f * drift_secs(v) * EN_SR;
            pad2_drift_left = frac2 * pad2_drift_total;
        }
        break;
    case P_ECHO: break;
    case P_ETIME: pl_mod_depth = (float)v * 0.01f; break;
    case P_REVERB: rev_send = 0.01f * (float)v; break;
    case P_SIZE: case P_REVSPACE: break;
    case P_DDIFF: case P_DMOTION: case P_DREV:
        rings_port_set_ambient(par[P_DDIFF], par[P_DMOTION], par[P_DREV]); break;
    case P_RMODEL: rings_port_set_model(v); break;
    case P_RSTRUCT: rings_port_set_structure(v); break;
    case P_RBRIGHT: rings_port_set_brightness(v); break;
    case P_RDAMP: rings_port_set_damping(v); break;
    case P_RPOS: rings_port_set_position(v); break;
    case P_RPOLY: rings_port_set_polyphony(v); break;
    case P_RFX: rings_port_set_string_fx(v); break;
    case P_M7SHAPE: case P_M8SHAPE: case P_M9SHAPE:
    case P_M10SHAPE: case P_M11SHAPE: {
        int idx=p==P_M7SHAPE?6:p==P_M8SHAPE?7:p==P_M9SHAPE?8:p==P_M10SHAPE?9:10;
        mod[idx].left=0;mod[idx].v=0;mod_phase[idx-6]=0;
        break;
    }
    case P_M7DEST: case P_M7DEPTH: case P_M7SPEED:
    case P_M8DEST: case P_M8DEPTH: case P_M8SPEED:
    case P_M9DEST: case P_M9DEPTH: case P_M9SPEED: break;
    case P_PADCHAR: pad_char = (float)v * 0.01f; break;
    case P_DRONEVOICE: pad2_handoff(); break;
    case P_REXCITER: rings_port_set_exciter(v); break;
    case P_PADCHORUS: pad_chorus = (float)v * 0.01f; break;
    case P_PADOCT: break;  /* pad_coefs reads it every control update: retunes immediately */
    case P_PAD2OCT: break;
    case P_PAD2CHAR: pad2_char = (float)v * 0.01f; break;
    case P_PAD2CHORUS: pad2_width = (float)v * 0.01f; break;
    case P_PADREV: pad_rev = (float)v * 0.01f; break;
    case P_RINGREV: ring_rev = (float)v * 0.01f; break;
    case P_RDELAYTIME:
    case P_RDELAYFB:
    case P_RDELAYMIX:
    case P_RDELAYREPEATS:
        rings_port_set_delay(par[P_RDELAYTIME], par[P_RDELAYFB], par[P_RDELAYMIX], par[P_RDELAYREPEATS]);
        break;
    case P_RSEQMODE: case P_EPATTERN:
        if (!en_playing) evo_select();
        break;
    case P_RBURST:
        break; /* read directly by the generative Rings scheduler */
    case P_DRONEREV: pad2_rev = (float)v * 0.01f; break;
    case P_BASSREV: break;  /* legacy */
    case P_BRIGHT: bright = (float)v; break;
    case P_SWELL: swell = (float)v; break;
    case P_EVOLVE: evolve = (float)v * 0.01f; break;
    case P_BTONE: pad_body = (float)v * 0.01f; break;
    case P_PADTONE: pad_handoff(); break;        /* the voices cross over as they did for a new key */
    case P_MOTION: motion = (float)v * 0.01f; break;
    case P_BOCT:                                 /* let it be heard now: a bell from the first loop */
        if (en_playing && nsteps && en_ring_seq_active_mode == 0) {
            int l, k;
            for (l = 0; l < EN_NLOOP; l++)
                for (k = 0; k < EN_LOOP_LEN[l]; k++)
                    if (loop_note[l][k] >= 0) {
                        bell_ring(l, k);
                        return;
                    }
        }
        break;
    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); rings_port_set_tune(v); fm_port_set_tune(v); break;
    case P_TEMPO:
        tempo_set();
        break;
    case P_ROOT:
        if (nsteps) {
            pad_retarget();
            pad_handoff();
            pad2_retarget();
            pad2_handoff();
        }
        break;
    case P_MIDI:
        if (!v) {
            fm_seq_midi_off_all();
            en_midi_out(0xB0, 123, 0);
            en_midi_out(0xB2, 123, 0);
        }
        break;
    default: break;
    }
}

static void pool_set(uint32_t p)
{
    pool = (p | 1u) & 0xFFFu;
    scale_set();
    if (deg >= nsteps)
        deg = 0;
    pad_retarget();
    pad_handoff();
    if (pad2_deg >= nsteps) pad2_deg = 0;
    pad2_retarget();
    pad2_handoff();
}

static void regrow(void)                        /* a new piece: fresh loops, home chord, patterns home */
{
    int l;
    fm_seq_regrow();
    prot_reset_position(0); prot_reset_position(1);
    for (l = 0; l < EN_NLOOP; l++) {
        loop_regrow(l);
        loop_rest[l] = 0;
    }
    deg = 0;
    pad2_deg = 0;
    pad_retarget();
    pad_handoff();
    pad2_retarget();
    pad2_handoff();
    pulse_rest = 0;
    en_pulse_rest = 0;
    drums_pattern();
    drift_reset();
    pad2_drift_reset();
    change_pending = 0;
}

static void notes_off(void)
{
    int i;
    for (i = 0; i < 5; i++)
        en_midi_out(0xB0 | (uint32_t)i, 123, 0);
    en_midi_out(0xB9, 123, 0);
}

static void quiet(void)
{
    monumatic_reverb_panic();
    int i;
    rings_wait_clear();
    rings_port_panic();
    fm_seq_stop();
    fm_port_panic();
    en_fm_level = 0.0f;
    for (i = 0; i < EN_NDRUM; i++)
        drum[i].on = 0, drum[i].wait = 0;
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].amp = 0.0f;
        pad2[i].amp = 0.0f;
    }
    notes_off();
}

void en_init(void)
{
    int i;
    en_playing=0;
    pad_movement_init();
    evo_init();
    en_mod_trace_head = en_mod_trace_count = 0;
    mod_trace_samples = 0;
    memset(mod_phase,0,sizeof(mod_phase));
    fm_mod_rng=0x38c62157u;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    rings_port_init();
    fm_port_init();
    for(int layer=0;layer<2;layer++)for(int ch=0;ch<2;ch++)classic_filter_init(&pad_filters[layer][ch]);
    rings_wait_clear();
    pad_ch_w = 0;
    pad_ch_phase = 0.0f;
    for (i = 0; i < PAD_CHORUS_N; i++) {
        pad_ch_l[i] = pad_ch_r[i] = 0;
    }
    plate_init();
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    for (i = 0; i < NMOD_PAD; i++) {
        pad2_mod[i].v = pad2_mod[i].tgt = 0.0f;
        pad2_mod[i].left = 0;
    }
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].note = pad[i].target = (float)(50 + 7 * i);
        pad[i].pan = PAD_PAN[i];
        pad[i].amp = 0.0f;
        pad[i].ph[0] = 0.0f;
        pad[i].ph[1] = 0.37f;
        pad[i].sub_ph = 0.19f * (float)(i + 1);
        pad[i].vib = 0.3f * (float)i;
        pad[i].midi_note = 0;
        pad[i].seq_shift = 0;
        pad[i].hf = 1.0f;
        pad[i].hand = 0;

        pad2[i].note = pad2[i].target = (float)(48 + 5 * i);
        pad2[i].pan = PAD_PAN[(i + 2) & 3];
        pad2[i].amp = 0.0f;
        pad2[i].peak = 0.0f;
        pad2_rest[i] = 0.0f;
        pad2[i].ph[0] = 0.17f * (float)(i + 1);
        pad2[i].ph[1] = 0.37f + 0.11f * (float)i;
        pad2[i].sub_ph = 0.13f * (float)(i + 1);
        pad2[i].vib = 0.17f + 0.23f * (float)i;
        pad2[i].midi_note = 0;
        pad2[i].seq_shift = 0;
        pad2[i].hf = 1.0f;
        pad2[i].hand = 0;
    }
    arcm = pad2_arcm = 0.0f;
    pad2_arc = pad2_arc_tgt = 0.0f;
    pad2_arc_left = 0;
    pad_gain = pad2_gain = 0.0f;
    pad2_deg = 0;
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    pool_set(EN_MODE_POOL[par[P_MODE]]);
    fm_seq_init();
    prot_init();
    ud_init();
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].note = pad[i].target;
        pad2[i].note = pad2[i].target;
    }
    for (i = 0; i < EN_NLOOP; i++) {
        loop_pos[i] = 0;
        loop_regrow(i);
    }
    drums_pattern();
    drift_reset();
    pad2_drift_reset();
}

/* ------------------------------------------------------------ transport --- */
static void clock_pulse(void)
{
    if (pulse == 0)
        tick();
    if (!en_ext)
        midi(0xF8, 0, 0);
    if (++pulse >= 6)
        pulse = 0;
}

static void play_start(int from_top)
{
    int i;
    if (from_top) {
        pulse = 0;
        sixteenth = 0;
        evo_next=0; evo_wrapped=0; en_evo_pos=0; evo_select();
        prot_reset_position(0); prot_reset_position(1);
        ud_reset(0); ud_reset(1);
        fm_seq_restart();
        for (i = 0; i < EN_NLOOP; i++)
            loop_pos[i] = 0;
    }
    pulse_left = 0.0f;
    en_playing = 1;
    pad_gate = pad2_gate = 1.0f;
    for (i = 0; i < EN_NPAD; i++) {              /* both Pad engines start independently */
        if (pad[i].amp < 1e-4f) {
            pad[i].t = START_T[i];
            pad[i].midi_note = 0;
        pad[i].seq_shift = 0;
            pad_breathe(&pad[i], i);
        }
        if (pad2[i].amp < 1e-4f) {
            pad2[i].t=0.0f;pad2[i].peak=0.0f;
            pad2[i].midi_note=0;pad2[i].seq_shift=12;
            pad2_rest[i]=(2.0f+1.75f*(float)i)*EN_SR;
        }
    }
    if (!en_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}

static void play_stop(void)
{
    en_playing = 0;
    evo_select();
    fm_seq_stop();
    fm_seq_select();
    fm_port_all_notes_off();
    pad_gate = pad2_gate = 0.0f;
    if (!en_ext)
        midi(0xFC, 0, 0);
    notes_off();
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 24) {
        case C_EVONEW: evo_randomize(); break;
        case C_FMEVONEW: fm_evo_randomize(); break;
        case C_PROTNEW: prot_new(a == 1); break;
        case C_FMNOTE: fm_port_trigger(a, b, 0); midi(0x92, (uint32_t)a, (uint32_t)b); break;
        case C_FMOFF: fm_port_note_off(a); midi(0x82, (uint32_t)a, 0); break;
        case C_FMALLOFF: fm_seq_stop(); fm_port_all_notes_off(); en_midi_out(0xB2, 123, 0); break;
        case C_SET:
            if (a < P_NPARAMS)
                apply(a, b);
            break;
        case C_POOL: pool_set((uint32_t)b & 0xFFFu); break;
        case C_PLAY:
            if (a && !en_playing)
                play_start(1);
            else if (!a && en_playing)
                play_stop();
            break;
        case C_NEXT:
            if (en_playing)
                change_pending = 1;
            else
                chord_change();
            break;
        case C_FREEZE: en_frozen = (uint8_t)(a != 0); break;
        case C_SEED:
            rng ^= (uint32_t)(b & 0xFFFF) * 2654435761u;
            pad2_rng ^= ((uint32_t)(b & 0xFFFF) + 0x9E37u) * 2246822519u;
            if (!rng) rng = 1u;
            if (!pad2_rng) pad2_rng = 1u;
            break;
        case C_CLOCK:
            en_ext = 1;
            ext_age = 0;
            if (a == EN_CLK_TICK) {
                if (en_playing)
                    clock_pulse();
            } else if (a == EN_CLK_START) {
                play_start(1);
            } else if (a == EN_CLK_CONTINUE) {
                if (!en_playing)
                    play_start(0);
            } else if (en_playing) {
                play_stop();
            }
            break;
        case C_PANIC: quiet(); break;
        case C_REGROW: regrow(); break;
        case C_RINGNOTE:
            /* Direct/manual Rings note. Schedule through the same queue used by
             * organic Density so simultaneous key presses cannot overwrite one
             * another before the next audio block. */
            ring_schedule(a, b, 0);
            break;
        }
    }
}

/* --------------------------------------------------------------- render --- */
#define BLK 256
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK];

/* the pad's bus: a 12 dB low-pass, darker than it sounds on paper (Brian Eno darkened everything: the
 * Synthi's filter below half, then tape). No chorus: a modulated delay against pure tones beats (on
 * the FM-1's speaker above all), and that read as pulsing. The width comes from the voices' places,
 * the echo's two heads and the plate. */
static float padl[BLK], padr[BLK];
static float plp[4];

static void pad_bus(uint32_t n)
{
    const float fc = fm_clampf(700.0f * fm_exp2f(bright * 0.03f + 0.6f * arcm), 300.0f, 9000.0f);
    const float c = 1.0f - fm_expf(-FM_TWO_PI * fc * (1.0f / EN_SR));
    const int vintage = par[P_PADTONE] != 0;
    const float wet = vintage ? pad_chorus * 0.42f : 0.0f;
    uint32_t j;
    for (j = 0; j < n; j++) {
        float l, r, cl = 0.0f, cr = 0.0f;
        plp[0] += c * (padl[j] - plp[0]);
        plp[1] += c * (plp[0] - plp[1]);
        plp[2] += c * (padr[j] - plp[2]);
        plp[3] += c * (plp[2] - plp[3]);
        l = plp[1];
        r = plp[3];
        if (wet > 1e-5f) {
            float ss = fm_sinf(pad_ch_phase * FM_TWO_PI);
            int dl = 650 + (int)(115.0f * ss);
            int dr = 650 - (int)(115.0f * ss);
            uint32_t il = (pad_ch_w + PAD_CHORUS_N - (uint32_t)dl) & (PAD_CHORUS_N - 1);
            uint32_t ir = (pad_ch_w + PAD_CHORUS_N - (uint32_t)dr) & (PAD_CHORUS_N - 1);
            cl = (float)pad_ch_l[il] * (1.0f / 32767.0f);
            cr = (float)pad_ch_r[ir] * (1.0f / 32767.0f);
            pad_ch_l[pad_ch_w] = pad_ch_q(l);
            pad_ch_r[pad_ch_w] = pad_ch_q(r);
            pad_ch_w = (pad_ch_w + 1u) & (PAD_CHORUS_N - 1);
            pad_ch_phase += (0.17f + 0.13f * pad_chorus) * (1.0f / EN_SR);
            if (pad_ch_phase >= 1.0f) pad_ch_phase -= 1.0f;
            l = l * (1.0f - wet) + cl * wet;
            r = r * (1.0f - wet) + cr * wet;
        }
        l=classic_filter_sample(&pad_filters[0][0],l);
        r=classic_filter_sample(&pad_filters[0][1],r);
        mixl[j] += l * 0.8f;
        mixr[j] += r * 0.8f;
        rsend[j] += (l + r) * 0.48f * pad_rev;
        esend[j] += (l + r) * 0.2f;
    }
    for (j = 0; j < 4; j++)
        plp[j] = fm_flush(plp[j]);
}

static __attribute__((noinline)) void render_pad(uint32_t n)
{
    int i;
    float lvl = 0.0f;
    uint32_t j0;
    for (j0 = 0; j0 < n; j0++)
        padl[j0] = padr[j0] = 0.0f;
    const float g = pad_vol * 0.5f, body_g = pad_vol * 0.18f * mod_pad_body;
    pad_gain += (pad_gate - pad_gain) * fm_minf((float)n / ((pad_gate > 0.0f ? 2.0f : 10.0f) * EN_SR), 1.0f);
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        float p0 = v->ph[0], p1 = v->ph[1], subph = v->sub_ph, amp = v->amp, sub_lp = v->sub_lp, s0 = v->ic[0], s1 = v->ic[1], s2 = v->ic[2], s3 = v->ic[3],
              gl, gr, pn, tgt = 0.0f, idx = 0.0f;
        uint32_t j;
        if (amp < 1e-5f && pad_gain < 1e-4f) {
            v->amp = 0.0f;
            continue;
        }
        pn = fm_clampf(v->pan + 0.25f * motion * mod_value((i + 3) % NMOD_PAD) + 0.24f*pad_motion_amount*pad_motion_wave[0][(i+2)&3], -1.0f, 1.0f);
        gl = fm_cosf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        gr = fm_sinf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        for (j = 0; j < n; j++) {
            float x, v1, v2, v3, b1, l1, b2;
            if (!(j & 31u)) {
                float br;
                pad_coefs(v, i);
                br = breath(v->t);
                tgt = br * v->peak * pad_gain * v->hf * (1.0f+0.12f*pad_motion_amount*pad_motion_wave[0][(i+3)&3]);
                /* the Rhodes blooms: more overtones as the breath swells (index in turns) */
                idx = (0.04f + 0.1f * bright * 0.01f + 0.1f * br + 0.035f*pad_motion_amount*pad_motion_wave[0][(i+1)&3]) * (1.0f + 0.4f * arcm);
            }
            if (!v->voices) {                    /* FM, the modulator at the carrier's pitch: a Rhodes */
                float m = sine(p0) * idx;
                x = (sine(p0 + m + 1.0f) + 0.12f * sine(2.0f * p0)) * 1.2f;   /* one carrier: no beating */
                p0 += v->inc[0];
                if (p0 >= 1.0f)
                    p0 -= 1.0f;
                amp += (tgt - amp) * 0.01f;
                x *= amp;
                padl[j] += x * gl;
                padr[j] += x * gr;
                {
                    float sub = sine(subph) * amp;
                    sub_lp += 0.025f * (sub - sub_lp);
                    padl[j] += sub_lp * body_g;
                    padr[j] += sub_lp * body_g;
                    subph += v->inc[0] * 0.5f;
                    if (subph >= 1.0f) subph -= 1.0f;
                }
                continue;
            }
            /* Vintage Analog: two free-running, gently detuned VCOs. A little triangle
             * rounds the band-limited saws before a 12 dB-ish warm low-pass. */
            {
                float ch = fm_clampf(mod_pad_char, 0.0f, 1.0f);
                float tri0 = p0 < 0.5f ? 4.0f * p0 - 1.0f : 3.0f - 4.0f * p0;
                float tri1 = p1 < 0.5f ? 4.0f * p1 - 1.0f : 3.0f - 4.0f * p1;
                float sawmix = 0.28f + 0.38f * ch;
                float trimix = 0.34f - 0.18f * ch;
                float o0 = sawmix * blep_saw(p0, v->inc[0]) + trimix * tri0;
                float o1 = (sawmix * 0.92f) * blep_saw(p1, v->inc[1]) + (trimix * 0.92f) * tri1;
                x = (o0 + o1) * (0.72f - 0.10f * ch);
            }
            p0 += v->inc[0]; if (p0 >= 1.0f) p0 -= 1.0f;
            p1 += v->inc[1]; if (p1 >= 1.0f) p1 -= 1.0f;
            v3 = x - s1;
            v1 = v->c1[0] * s0 + v->c1[1] * v3;
            v2 = s1 + v->c1[1] * s0 + v->c1[2] * v3;
            s0 = 2.0f * v1 - s0;
            s1 = 2.0f * v2 - s1;
            amp += (tgt - amp) * 0.01f;
            x = v2 * amp * 0.82f;
            padl[j] += x * gl;
            padr[j] += x * gr;
            {
                float sub = sine(subph) * amp;
                sub_lp += 0.025f * (sub - sub_lp);
                padl[j] += sub_lp * body_g;      /* Low Body stays centred below the moving pad */
                padr[j] += sub_lp * body_g;
                subph += v->inc[0] * 0.5f;
                if (subph >= 1.0f) subph -= 1.0f;
            }
        }
        v->ic[0] = fm_flush(s0);
        v->ic[1] = fm_flush(s1);
        v->ic[2] = fm_flush(s2);
        v->ic[3] = fm_flush(s3);
        v->sub_lp = fm_flush(sub_lp);
        v->amp = amp;
        v->ph[0] = p0;
        v->ph[1] = p1;
        v->sub_ph = subph;
        en_pad_note[i] = (uint8_t)en_pad_pitch((int)v->note, v->seq_shift, par[P_PADOCT]);
        en_pad_amp[i] = amp;
        lvl = fm_maxf(lvl, amp);
    }
    en_pad_level = lvl;
    pad_bus(n);
}


static void pad2_bus(uint32_t n)
{
    /* One pole per side is enough here because Pad 2 Lite's oscillator palette is
     * already soft. Coefficient is a bilinear-style approximation to a one-pole
     * low-pass, avoiding exp() in the audio/control path. */
    const float fc = fm_clampf(520.0f + 5200.0f * mod_pad2_bright + 700.0f * pad2_arcm, 280.0f, 6200.0f);
    const float c = fc / (fc + 7000.0f);
    const float width = 1.0f + 0.55f * pad2_width;
    uint32_t j;
    for (j = 0; j < n; j++) {
        float l, r, mid, side;
        pad2_plp[0] += c * (padl[j] - pad2_plp[0]);
        pad2_plp[2] += c * (padr[j] - pad2_plp[2]);
        l = pad2_plp[0];
        r = pad2_plp[2];

        /* Phase-safe width: no delay line, no LFO, no combing. */
        mid = 0.5f * (l + r);
        side = 0.5f * (l - r) * width;
        l = mid + side;
        r = mid - side;

        l=classic_filter_sample(&pad_filters[1][0],l);
        r=classic_filter_sample(&pad_filters[1][1],r);
        mixl[j] += l * 0.8f;
        mixr[j] += r * 0.8f;
        rsend[j] += (l + r) * 0.48f * pad2_rev;
        esend[j] += (l + r) * 0.2f;
    }
    pad2_plp[0] = fm_flush(pad2_plp[0]);
    pad2_plp[2] = fm_flush(pad2_plp[2]);
}

static inline float pad2_tri(float ph)
{
    return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
}

static __attribute__((noinline)) void render_pad2(uint32_t n)
{
    int i;
    float lvl = 0.0f;
    uint32_t j0;
    for (j0 = 0; j0 < n; j0++)
        padl[j0] = padr[j0] = 0.0f;

    {
        const float g = pad2_vol * 0.5f;
        const float body_g = pad2_vol * 0.13f * mod_pad2_body;
        pad2_gain += (pad2_gate - pad2_gain) * fm_minf((float)n / ((pad2_gate > 0.0f ? 4.0f : 20.0f) * EN_SR), 1.0f);

        for (i = 0; i < EN_NPAD; i++) {
            pad_t *v = &pad2[i];
            float p0 = v->ph[0], p1 = v->ph[1], subph = v->sub_ph, amp = v->amp;
            float gl, gr, pn, tgt = 0.0f;
            float glass_air=0.10f+0.055f*pad_motion_amount*pad_motion_wave[1][(i+1)&3];
            uint32_t j;
            if (amp < 1e-5f && pad2_gain < 1e-4f) {
                v->amp = 0.0f;
                continue;
            }

            pn = fm_clampf(v->pan + 0.20f * motion * pad2_mod_value((i + 3) % NMOD_PAD) + 0.38f*pad_motion_amount*pad_motion_wave[1][(i+2)&3], -1.0f, 1.0f);
            /* Linear pan is intentionally used for Lite: the bus Width stage restores
             * stereo size and this saves two trig calls for every voice/render chunk. */
            gl = (0.72f - 0.28f * pn) * g;
            gr = (0.72f + 0.28f * pn) * g;

            for (j = 0; j < n; j++) {
                float x, t0, t1;
                if (!(j & 127u)) {
                    float br;
                    pad2_coefs_lite(v, i);
                    br = breath(v->t);
                    tgt = br * v->peak * pad2_gain * v->hf * (1.0f+0.18f*pad_motion_amount*pad_motion_wave[1][(i+3)&3]);
                }

                t0 = pad2_tri(p0);
                t1 = pad2_tri(p1);
                if (!v->voices) {
                    /* Glass: a clean sine body with a light detuned triangle and a
                     * cheap octave harmonic. Bright and airy, but still Pad-2-Lite. */
                    float hph = p0 + p0;
                    if (hph >= 1.0f) hph -= 1.0f;
                    x = 0.76f * sine(p0) + 0.16f * t1 + glass_air * pad2_tri(hph);
                } else {
                    /* Strings: two wider-detuned soft triangles. Character increases
                     * spread and slightly changes the blend; the bus LP keeps it soft. */
                    float ch = fm_clampf(pad2_char, 0.0f, 1.0f);
                    x = (t0 * (0.53f + 0.05f * ch) + t1 * (0.53f - 0.03f * ch)) * 0.76f;
                }

                p0 += v->inc[0]; if (p0 >= 1.0f) p0 -= 1.0f;
                p1 += v->inc[1]; if (p1 >= 1.0f) p1 -= 1.0f;
                amp += (tgt - amp) * 0.005f;
                x *= amp;
                padl[j] += x * gl;
                padr[j] += x * gr;

                if (body_g > 1e-6f) {
                    /* Cheap octave-down body: triangle rather than sine + per-voice LP.
                     * The Pad 2 bus low-pass softens it immediately afterwards. */
                    float sub = pad2_tri(subph) * amp * body_g;
                    padl[j] += sub;
                    padr[j] += sub;
                    subph += v->inc[0] * 0.5f;
                    if (subph >= 1.0f) subph -= 1.0f;
                }
            }

            v->amp = amp;
            v->ph[0] = p0;
            v->ph[1] = p1;
            v->sub_ph = subph;
            if (amp > lvl) lvl = amp;
        }
    }
    en_drone_level = lvl;                        /* legacy public meter name = Pad 2 */
    pad2_bus(n);
}

static void render_bells(uint32_t n)
{
    int i;
    const float g = bell_vol * 0.62f, arate = 0.0004f + 0.0056f * fm_minf(btone * 2.0f, 1.0f);
    for (i = 0; i < NBELL; i++) {
        bell_t *v = &bell[i];
        float cph = v->cph, mph = v->mph, amp = v->amp, idx = v->idx, att = v->att;
        uint32_t j = 0;
        if (!v->on)
            continue;
        if (v->wait > 0) {
            if ((uint32_t)v->wait >= n) {
                v->wait -= (int32_t)n;
                continue;
            }
            j = (uint32_t)v->wait;
            v->wait = 0;
        }
        for (; j < n; j++) {
            float m = sine(mph) * idx, x;
            float p = cph + m + 4.0f;            /* + 4 turns: positive for the table */
            x = sine(p) * amp * att * g;
            mixl[j] += x * v->gl;
            mixr[j] += x * v->gr;
            rsend[j] += x * 0.7f;
            esend[j] += x * 0.85f;
            cph += v->cinc;
            mph += v->minc;
            if (cph >= 1.0f)
                cph -= 1.0f;
            if (mph >= 1.0f)
                mph -= 1.0f;
            amp *= v->adec;
            idx *= v->idec;
            att += (1.0f - att) * arate;         /* the strike: ~4 ms, or soft as a slowed piano (~50 ms) */
        }
        v->cph = cph;
        v->mph = mph;
        v->amp = amp;
        v->idx = idx;
        v->att = att;
        if (amp < 0.0008f) {
            v->on = 0;
            midi(0x81, v->note, 0);
        }
    }
}

/* a hat: the metal and noise, high-passed (Chamberlin SVF, ~4 to 10 kHz by Hats); no pitch, little echo */
static void render_hat(drum_t *v, uint32_t n, float g, float pl, float pr)
{
    uint32_t j;
    int k;
    float x1, x2, amp, att;
    if (!v->on)
        return;
    x1 = v->x1, x2 = v->x2, amp = v->amp, att = v->att;
    for (j = 0; j < n; j++) {
        float in = 0.0f, hp, x;
        for (k = 0; k < 4; k++) {
            v->mph[k] += HAT_INC[k];
            if (v->mph[k] >= 1.0f)
                v->mph[k] -= 1.0f;
            in += v->mph[k] < 0.5f ? 0.25f : -0.25f;
        }
        noise = noise * 1664525u + 1013904223u;
        in = 0.5f * in + 0.6f * (float)(int32_t)noise * (1.0f / 2147483648.0f);
        hp = in - x2 - 0.8f * x1;
        x1 += hat_f * hp;
        x2 += hat_f * x1;
        x = hp * amp * att * g;
        mixl[j] += x * pl;
        mixr[j] += x * pr;
        rsend[j] += x * 0.2f;
        esend[j] += x * 0.08f;
        amp *= v->adec;
        att += (1.0f - att) * 0.3f;
    }
    v->x1 = fm_flush(x1);
    v->x2 = fm_flush(x2);
    v->amp = amp;
    v->att = att;
    v->on = amp > 0.0005f;
}

static void render_drums(uint32_t n)
{
    uint32_t j;
    const float g = pulse_vol * 1.1f;
    drum_t *v;
    v = &drum[0];                                /* thump: a sine falling to its note */
    if (v->on) {
        float ph = v->ph, inc = v->inc, amp = v->amp, att = v->att;
        for (j = 0; j < n; j++) {
            float x = sine(ph) * amp * att * g;
            mixl[j] += x;
            mixr[j] += x;
            rsend[j] += x * 0.1f;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            inc += (v->inc_end - inc) * 0.0012f;
            amp *= v->adec;
            att += (1.0f - att) * 0.02f;
        }
        v->ph = ph;
        v->inc = inc;
        v->amp = amp;
        v->att = att;
        v->on = amp > 0.0005f;
    }
    render_hat(&drum[1], n, g * 0.2f, 0.6f, 1.0f);    /* closed: a little right */
    render_hat(&drum[2], n, g * 0.14f, 1.0f, 0.7f);   /* open: a little left */
}

/* v17 master peak guard. This is exactly linear through +/-0.90 and only
 * compresses genuine overload above that point. Unlike the old always-on tanh
 * or v16 hard clamp, normal program material never passes through a nonlinear
 * transfer curve. */
static inline float clean_peak_guard(float x)
{
    const float t = 0.90f;
    const float h = 1.0f - t;
    float a = x < 0.0f ? -x : x;
    if (a <= t)
        return x;
    {
        float d = a - t;
        float y = t + h * d / (h + d);
        return x < 0.0f ? -y : y;
    }
}

/* CPU1 renders Rings and the independent FM layer into these buffers; CPU0 pads
 * use their original mix buffers. All voice parameter updates finish before submission; completion is joined
 * before the next control tick, trigger, preset change or flash operation. */
static float dc_left[BLK], dc_right[BLK], dc_send[BLK];
static struct { uint32_t n; float send, fm_send; } dc_ring_job;
static void dc_render_rings(void *unused)
{
    uint32_t j, n = dc_ring_job.n;
    (void)unused;
    for (j = 0; j < n; ++j) dc_left[j] = dc_right[j] = dc_send[j] = 0.0f;
    rings_port_render_add(dc_left, dc_right, dc_send, dc_ring_job.send, n);
    fm_port_render_add(dc_left, dc_right, dc_send, dc_ring_job.fm_send, n);
}

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = 0.0f;
    dc_ring_job.n = n; dc_ring_job.send = ring_rev;
    dc_ring_job.fm_send = (float)par[P_FMREV] * 0.01f;
    int submitted = dc_submit(dc_render_rings, 0);
    render_pad(n);
    render_pad2(n);
    int ready = 0;
    if (submitted) ready = dc_wait();
    else if (dc_status() != 3) {
        /* Use the same source sum as CPU1: pad + (Rings + FM). A direct
         * add into the pad mix changes rounding and the reverb's input. */
        dc_render_rings(0);
        ready = 1;
    }
    if (ready) for (j = 0; j < n; ++j) {
        mixl[j] += dc_left[j]; mixr[j] += dc_right[j]; rsend[j] += dc_send[j];
    } /* after a worker timeout Rings and FM stay muted until reboot */
    en_fm_level = fm_port_level;
    /* Monumatic processes the shared mono send at full 44.1 kHz.
     * rsend becomes wet L and esend becomes wet R. */
    monumatic_reverb_process(rsend, esend, n);
    {
        /* Preserve WET2's true wet/dry curve. Monumatic wet is unity gain;
         * the GP5-specific 1.45 boost is intentionally removed. */
        const float rv = fm_clampf(rev_send, 0.0f, 1.0f);
        const float wet_curve = rv * (1.35f - 0.35f * rv);
        const float wet_gain = wet_curve;
        const float dry_gain = 1.0f - wet_curve;
        float peak = 0.0f;
        for (j = 0; j < n; j++) {
            float a = rsend[j] < 0.0f ? -rsend[j] : rsend[j];
            float b = esend[j] < 0.0f ? -esend[j] : esend[j];
            if (a > peak) peak = a;
            if (b > peak) peak = b;
        }
        /* Meter the audible wet return rather than the raw tank output. */
        en_reverb_level = fm_maxf(peak * wet_gain, en_reverb_level * 0.92f);
        for (j = 0; j < n; j++) {
            /* Wet/dry crossfade. At zero the original mix is untouched; as
             * REVERB rises the direct path is progressively reduced, reaching
             * fully wet at 100. The clean peak guard below still protects
             * exceptional summed peaks after the crossfade. */
            mixl[j] = mixl[j] * dry_gain + rsend[j] * wet_gain;
            mixr[j] = mixr[j] * dry_gain + esend[j] * wet_gain;

        /* The actual overload fix: preserve about 3 dB more headroom than v16
         * before the hardware-volume gain. Normal material stays perfectly
         * linear. Only exceptional summed peaks above +/-0.90 touch the smooth
         * peak guard; there is no hard clipping and no always-on saturation. */
        float l = clean_peak_guard(mixl[j] * 0.34f * gain);
        float r = clean_peak_guard(mixr[j] * 0.34f * gain);
        out[2 * j] = (int32_t)(l * 8388607.0f);
        out[2 * j + 1] = (int32_t)(r * 8388607.0f);
        }
    }
}

/* the arc: a slow tide, minutes long, between sparse (-1) and full (+1); Motion sets how far it goes */
static float arc, arc_tgt;
static uint32_t arc_left;

static void control(uint32_t n)                 /* once a chunk: the slow things */
{
    int i;
    mods_step(n);
    pad2_mods_step(n);
    pad2_control_clock(n);
    pad_movement_step(n);
    for(int layer=0;layer<2;layer++){
        static const int ids[2][5]={{P_FFILTERMODEL,P_FFILTERTYPE,P_FFILTERCUT,P_FFILTERRES,P_FFILTERMIX},{P_P2FILTERMODEL,P_P2FILTERTYPE,P_P2FILTERCUT,P_P2FILTERRES,P_P2FILTERMIX}};
        const int *id=ids[layer];
        float cut=mod[9].v*(float)par[P_M10DEPTH]*0.01f;
        float res=mod[10].v*(float)par[P_M11DEPTH]*0.01f;
        cut+=(layer?0.85f:0.65f)*pad_motion_amount*pad_motion_wave[layer][0];
        for(int ch=0;ch<2;ch++)classic_filter_set(&pad_filters[layer][ch],
            par[id[0]],par[id[1]],par[id[2]],par[id[3]],par[id[4]],cut,res);
    }
    mod_pad2_bright = fm_clampf(pad2_bright + 0.45f * route_mod(8) + 0.22f*pad_motion_amount*pad_motion_wave[1][1], 0.0f, 1.0f);
    mod_pad2_body = fm_clampf(pad2_body + 0.45f * route_mod(9), 0.0f, 1.0f);
    mod_pad_body = fm_clampf(pad_body + 0.45f * route_mod(10), 0.0f, 1.0f);
    mod_pad_char = fm_clampf(pad_char + 0.45f * route_mod(11), 0.0f, 1.0f);
    mod_rev_size = fm_clampf((float)par[P_SIZE] * 0.01f + 0.45f * route_mod(12), 0.0f, 1.0f);
    mod_rev_damp = 0.45f * route_mod(13);
    monumatic_reverb_set(mod_rev_size, fm_clampf((float)par[P_GP5TONE] * 0.01f - mod_rev_damp, 0.0f, 1.0f), pl_mod_depth);
    rings_port_set_modulation(
        mod[6].v, par[P_M7DEST], par[P_M7DEPTH],
        mod[7].v, par[P_M8DEST], par[P_M8DEPTH],
        mod[8].v, par[P_M9DEST], par[P_M9DEPTH]);
    if (arc_left <= n) {
        float r = frand() * 2.0f - 1.0f;
        arc_tgt = r * (0.6f + 0.4f * frand()) + (r > 0.0f ? 0.25f : -0.25f);   /* away from the middle */
        arc_left = (uint32_t)((240.0f + 360.0f * frand()) * EN_SR);
    } else {
        arc_left -= n;
    }
    if (en_playing)
        arc += (fm_clampf(arc_tgt, -1.0f, 1.0f) - arc) * (float)n * (1.0f / (90.0f * EN_SR));
    arcm = arc * motion;
    en_arc = arcm;
    if (en_playing && !en_frozen && !change_pending) {
        drift_left -= (float)n;
        if (drift_left <= 0.0f)
            change_pending = 1;
    }
    en_drift = drift_total > 0.0f ? 1.0f - fm_clampf(drift_left / drift_total, 0.0f, 1.0f) : 0.0f;
    for (i = 0; i < EN_NLOOP; i++)
        en_loop_glow[i] *= 0.985f;
    for (i = 0; i < EN_NDRUM; i++)
        en_drum_glow[i] *= 0.95f;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    drain();
    if (en_ext && ++ext_age > 100u)
        en_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        int timed = en_playing && !en_ext;
        if (timed) {
            if (pulse_left <= 0.0f) {
                clock_pulse();
                pulse_left += step_len * (1.0f / 6.0f);
            }
            if ((float)k > pulse_left)
                k = (uint32_t)pulse_left + 1u;
        }
        if (k > n)
            k = n;
        /* A tick may have just posted humanized Rings strikes. Fire zero-delay
         * events before rendering, then split the block at the next strike so
         * every event is consumed as a distinct Rings strum. */
        rings_fire_due();
        fm_seq_fire_due();
        {
            int32_t rw = rings_wait_next();
            if (rw > 0 && (uint32_t)rw < k)
                k = (uint32_t)rw;
            int32_t fw = fm_seq_wait_next();
            if (fw > 0 && (uint32_t)fw < k)
                k = (uint32_t)fw;
        }
        if (timed)
            pulse_left -= (float)k;
        control(k);
        render_chunk(out, k, gain);
        rings_wait_step((int32_t)k);
        fm_seq_step((int32_t)k);
        out += 2 * k;
        n -= k;
    }
}

#ifdef PLAT_HOST
#include <assert.h>
#include <stdio.h>
#include "../../tests/hallucigenia/host.h"
#include "../../tests/dusk/pad_roles_host.h"
#include "../../tests/dusk/pad_movement_host.h"
#endif
