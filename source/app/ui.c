#include "dsp/evolve_rate.h"
/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   keys           the scale: each key adds or removes its note from the pool (the root stays)
 *   PLAY           the garden grows / rests        ARP   the next chord, now
 *   REC            FREEZE: the chord holds, the reverb rings on    OCT- / OCT+   Pad/Rings octave by page
 *   ENV Pad 1      ARP Pad 2      SEQ Rings      FX Reverb      GLO Mixer/Setup
 *   SEL FM pages   SELECT root note on HOME (HOME+SELECT switches HOME row)   ALGORITHM scale/mode   PRESETS the piece
 *   HOME Garden    LFO Mod; all other hardware buttons retain their existing actions
 *   KNOB 1-4       the page's four values
 *   SAVE           saves (it also saves by itself a few seconds after a change, while resting);
 *                  held 1.5 s: the piece back to its recipe */
#include "fastmath.h"
#include "dual_core.h"
#include "perf.h"
#include "fm_port.h"

/* the pages, one a layer (a button each); page buttons handle navigation directly */
enum { V_GARDEN, V_PAD, V_PADBODY, V_RINGS, V_RINGTONE, V_RINGEXC, V_RINGDLY, V_MOD, V_MIXER, V_DRONE, V_DRONEVOICE, V_SPACE, V_REVSEND, V_SETUP, V_PADSEQ, V_PAD2SEQ, V_RINGSEQ, V_PERF, V_DAMBIENT, V_ESETUP, V_EPATTERN, V_FM, V_FMMOTION, V_FMSEQ, V_FMESETUP, V_FMEPATTERN, V_REVMIX, V_RPROTOPT, V_FPROTOPT, V_FMFILTER, V_RUNITOPT, V_FUNITOPT, V_ROOTDIV, V_PAD2FILTER, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Garden", "Pad 1", "Body", "Rings", "Tone", "Exciter", "Delay", "Mod", "Mixer", "Pad 2", "Body", "Reverb", "Sends", "Setup", "Pad 1 Seq", "Pad 2 Seq", "Rings Seq", "Performance", "Delay 2", "Hallucigenia Setup", "Hallucigenia Pattern", "FM", "FM Motion", "FM Seq", "FM Hallucigenia Setup", "FM Hallucigenia Pattern", "Reverb Mix", "Proteus Options", "FM Proteus Options", "Pad 1 Filter", "Unit-D Options", "FM Unit-D Options", "Root & Note Div", "Pad 2 Filter"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_ENV, B_ENV, B_SEQ, B_SEQ, B_SEQ, B_SEQ, B_LFO, B_GLO, B_ARP, B_ARP, B_FX, B_FX, B_GLO, B_ENV, B_ARP, B_SEQ, B_GLO, B_SEQ, B_SEQ, B_SEQ, B_SEL, B_SEL, B_SEL, B_SEL, B_SEL, B_FX, B_SEQ, B_SEL, B_ENV, B_SEQ, B_SEL, B_HOME, B_ARP};
static const char *const VIEW_BTN_NAME[NVIEWS] = {"HOME", "PAD 1", "PAD 1", "RINGS", "RINGS", "RINGS", "RINGS", "LFO", "MIXER", "PAD 2", "PAD 2", "REVERB", "REVERB", "MIXER", "PAD 1", "PAD 2", "RINGS", "MIXER", "RINGS", "RINGS", "RINGS", "FM", "FM", "FM", "FM", "FM", "REVERB", "RINGS", "FM", "PAD 1", "FM", "HOME", "PAD 2"};
enum { K_LEDS = 192, K_CPU, K_MOD_ROUTE, K_MOD_DEST, K_MOD_DEPTH, K_MOD_SPEED, K_PERF_MODE, K_PERF_RESET, K_PERF_SECS, K_PERF_STATE, K_EVONEW, K_FMEVONEW, K_RPROTNEW, K_FPROTNEW, K_MOD_SHAPE, K_NONE };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_TEMPO, P_EVOLVE, P_MOTION, P_DRIFT},
    {P_SWELL, P_BRIGHT, P_PADTONE, P_PADCHORUS},
    {P_BTONE, P_PADCHAR, K_NONE, K_NONE},
    {P_RMODEL, P_RBURST, P_RPOLY, K_NONE},
    {P_RSTRUCT, P_RBRIGHT, P_RDAMP, P_RPOS},
    {P_REXCITER, K_NONE, K_NONE, K_NONE},
    {P_RDELAYTIME, P_RDELAYFB, P_RDELAYMIX, P_RDELAYREPEATS},
    {K_MOD_ROUTE, K_MOD_DEST, K_MOD_DEPTH, K_MOD_SPEED},
    {P_PAD, P_PULSE, P_BELLS, P_FMLEVEL},
    {P_HITS, P_SWING, P_DRONEVOICE, P_PAD2CHORUS},
    {P_HATS, P_PAD2CHAR, K_NONE, K_NONE},
    {P_SIZE, P_GP5TONE, P_ETIME, P_REVERB},
    {P_PADREV, P_RINGREV, P_DRONEREV, P_FMREV},
    {P_TUNE, P_MIDI, K_LEDS, K_NONE},
    {P_PADOCT, P_PADRANGE, P_PADCHANCE, K_NONE},
    {P_PAD2OCT, P_PAD2RANGE, P_PAD2CHANCE, K_NONE},
    {P_RSEQMODE, P_BOCT, P_RINGRANGE, P_RINGCHANCE},
    {K_PERF_MODE, K_PERF_RESET, K_PERF_SECS, K_PERF_STATE},
    {P_DDIFF, P_DMOTION, P_DREV, K_NONE},
    {P_ELENGTH, P_EMUTATE, P_EDENSITY, P_ERATE},
    {P_ERANGE, P_EFOLLOW, P_EPATTERN, K_EVONEW},
    {P_FMPATCH, P_FMLEVEL, P_FMPOLY, P_FMGATE},
    {K_NONE, P_FMBURST, P_FMREV, K_NONE},
    {P_FMSEQMODE, P_FMOCT, P_FMRANGE, P_FMCHANCE},
    {P_FMELENGTH, P_FMEMUTATE, P_FMEDENSITY, P_FMERATE},
    {P_FMERANGE, P_FMEFOLLOW, P_FMEPATTERN, K_FMEVONEW},
    {P_SIZE, P_GP5TONE, P_ETIME, P_REVERB},
    {P_RPROTRANGE, K_RPROTNEW, P_BOCT, P_RINGRANGE},
    {P_FPROTRANGE, K_FPROTNEW, P_FMOCT, P_FMRANGE},
    {P_FFILTERTYPE,P_FFILTERCUT,P_FFILTERRES,P_FFILTERMIX},
    {P_RUDTOL,P_RUDRANGE,P_RUDRATE,P_RUDPOLY},
    {P_FUDTOL,P_FUDRANGE,P_FUDRATE,P_FUDPOLY},
    {P_ROOT, P_FMDENSITY, P_DENSITY, K_NONE},
    {P_P2FILTERTYPE,P_P2FILTERCUT,P_P2FILTERRES,P_P2FILTERMIX},
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Tempo", "Evolve", "Motion", "Drift"},
    {"Swell", "Bright", "Colour", "Chorus"},
    {"Low Body", "Character", "", ""},
    {"Model", "Burst", "Polyphony", ""},
    {"Structure", "Brightness", "Damping", "Position"},
    {"Exciter", "", "", ""},
    {"Time", "Feedback", "Mix", "Repeats"},
    {"Source", "Destination", "Depth", "Speed"},
    {"Pad", "Pad 2", "Rings", "FM"},
    {"Swell", "Bright", "Colour", "Width"},
    {"Low Body", "Character", "", ""},
    {"Decay", "Tone", "Motion", "Dry/Wet"},
    {"Pad", "Rings", "Pad 2", "FM"},
    {"Tune", "MIDI out", "Lights", ""},
    {"Octave", "Oct Drift", "Change", ""},
    {"Octave", "Oct Drift", "Change", ""},
    {"Mode", "Octave", "Oct Drift", "Chance"},
    {"Mode", "Reset", "Seconds", "Worker"},
    {"Diffusion", "Motion", "Rev Send", ""},
    {"Length", "Mutation", "Density", "Rate"},
    {"Range", "Follow", "Pattern", "New"},
    {"Patch", "Level", "Voices", "Gate"},
    {"Density", "Burst", "Rev Send", ""},
    {"Mode", "Octave", "Oct Drift", "Chance"},
    {"Length", "Mutation", "Density", "Rate"},
    {"Range", "Follow", "Pattern", "New"},
    {"Decay", "Tone", "Motion", "Dry/Wet"},
    {"Range", "New Melody", "Octave", "Oct Drift"},
    {"Range", "New Melody", "Octave", "Oct Drift"},
    {"Type", "Cutoff", "Resonance", "Mix"},
    {"Tolerance", "Range", "Rate", "Polyphony"},
    {"Tolerance", "Range", "Rate", "Polyphony"},
    {"Root", "FM Density", "Rings Density", ""},
    {"Type", "Cutoff", "Resonance", "Mix"},
};

/* the screen's colours: the cover of Ambient 1: Music for Airports (1978), as the web app and the site.
 * The map's paper, its ink; the land's greens for the pad (the chord) and the scale, its rivers for the
 * bells; the bass and the pulse in darker inks. Each layer has a pale tint for the shapes at rest. */
#define K_BG RGB(2, 5, 7)
#define K_PANEL RGB(8, 14, 17)
#define K_LINE RGB(112, 132, 138)
#define K_DIM RGB(162, 174, 176)
#define K_TEXT RGB(248, 248, 244)
#define K_ACC RGB(20, 220, 232)
#define K_BELL RGB(232, 30, 216)
#define K_BELL_T RGB(86, 32, 86)
#define K_PAD RGB(22, 218, 232)
#define K_PAD_T RGB(25, 74, 82)
#define K_PULSE RGB(244, 210, 30)
#define K_PULSE_T RGB(78, 68, 18)
#define K_BASS RGB(244, 58, 66)
#define K_BASS_T RGB(80, 25, 28)
#define K_FM RGB(255, 154, 42)
#define K_FM_T RGB(84, 51, 19)
#define K_FROST RGB(32, 222, 174)
#define K_FROST_T RGB(24, 72, 60)


static struct {
    uint8_t view;
    uint8_t mod_route;
    uint8_t mod_row, fm_filter_row; /* SELECT row 1/2 for M7..M11 and FM Filter */
    uint8_t seq_zone[2]; /* SELECT: mode (0), upper four (1), lower four (2) */
    uint8_t unit_row[2]; /* UNIT-D options: SELECT Row 1/2, blank slots reserved */
    uint8_t grid_row[NVIEWS]; /* row highlight for the standard 2x4 pages */
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t help;                      /* a button's card is up: the button + 1 */
    uint8_t reset_done;                /* SAVE held: the piece went back to its recipe this hold */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[48];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint8_t master_seen;               /* MASTER popup: ignore the boot-time position */
    uint8_t master_pct;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
} ui;

/* Track the emitted pitch, not the current octave, until its source releases.
 * A zero entry is idle; MIDI note 0..127 is stored as 1..128. */
static uint8_t fm_key_note[NKEYS], fm_midi_note[128];
static int fm_view(int v) { return (v >= V_FM && v <= V_FMEPATTERN) || v == V_FPROTOPT || v == V_FUNITOPT; }
static int fm_clip_note(int n) { return n < 0 ? 0 : n > 127 ? 127 : n; }
static void fm_manual_release(void)
{
    int i, held = 0;
    for (i = 0; i < NKEYS; ++i) held |= fm_key_note[i];
    for (i = 0; i < 128; ++i) held |= fm_midi_note[i];
    /* One audio command leaves room for a complete piece load even when a
     * MIDI sender has left many source keys held beyond the voice count. */
    if (held) en_fm_all_notes_off();
    memset(fm_key_note, 0, sizeof fm_key_note);
    memset(fm_midi_note, 0, sizeof fm_midi_note);
}

/* the screen reader (announce.c; speak() below): a message went to it this frame, so speak() leaves what it
 * changed be; and the transport as last said (PLAY says its own: the engine takes it a block later) */
static uint8_t sp_said;
static struct {
    uint8_t playing;
    uint32_t play_t;                   /* when PLAY (or the start) set it: the engine catches up within ms */
} sp_tp;
static void sp_piece(void);
static void msg_text(const char *a, const char *b, char *t)   /* the message as a reader gets it: "a b" */
{
    uint32_t i, k = 0;
    for (i = 0; i < 23u && a && a[i]; i++)
        t[k++] = a[i];
    if (b && b[0])
        t[k++] = ' ';
    for (i = 0; i < 23u && b && b[i]; i++)
        t[k++] = b[i];
    while (k && t[k - 1] == '.')               /* "UPDATE MODE IN 3..": the dots are for the eye */
        k--;
    t[k] = 0;
}
/* a message for a moment in the header; flags: how a screen reader gets it (0: the caller tells it) */
static void say_as(const char *a, const char *b, uint32_t flags)
{
    char t[48];
    msg_text(a, b, ui.msg);
    ui.msg_until = plat_ms() + 1300u;
    if (flags && ann_on()) {
        msg_text(a, b, t);
        ann_say(t, 0, flags);
        sp_said = 1;
    }
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
/* something did not happen: a screen reader says it whole (what follows does not cut it off) */
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

static int pool_count(uint32_t p)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return n;
}
static uint32_t pool_abs(void)                 /* the pool as absolute pitch classes */
{
    uint32_t p = proj.pool, r = (uint32_t)proj.par[P_ROOT];
    return ((p << r) | (p >> (12u - r))) & 0xFFFu;
}
static const char *mode_name(void)
{
    if (proj.par[P_MODE] == MODE_PIANO)
        return "Piano";
    return proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? EN_MODE_NAME[proj.par[P_MODE]] : "Custom";
}

/* ------------------------------------------------------------ values --- */
static int fm_filter_knob(int view,int row,int slot) {
    static const int upper[4]={P_FFILTERTYPE,P_FFILTERCUT,P_FFILTERRES,P_FFILTERMIX};
    if(view==V_PAD2FILTER){static const int p2[4]={P_P2FILTERTYPE,P_P2FILTERCUT,P_P2FILTERRES,P_P2FILTERMIX};return row==2?(slot==0?P_P2FILTERMODEL:K_NONE):p2[slot];}
    return row==2 ? slot==0?P_FFILTERMODEL:K_NONE : upper[slot];
}
static int mod_view_knob(int row,int slot) {
    if (ui.mod_route < 11) {
        static const int top[4]={K_MOD_ROUTE,K_MOD_DEST,K_MOD_DEPTH,K_NONE};
        static const int bottom[4]={K_MOD_SHAPE,K_NONE,K_MOD_SPEED,K_NONE};
        return row==2?bottom[slot]:top[slot];
    }
    return VIEW_KNOB[V_MOD][slot];
}
/* Standard two-by-four pages. Old single-row pages still retain exactly the
 * same parameter IDs, while the extra four positions are visible blank slots.
 * SELECT chooses the highlighted row except on HOME: there it changes Root,
 * and HOME+SELECT chooses a row so Root control is never lost. */
static int grid_page(int v)
{
    switch(v) {
    case V_GARDEN: case V_ROOTDIV: case V_PAD: case V_PADBODY: case V_PADSEQ:
    case V_DRONE: case V_DRONEVOICE: case V_PAD2SEQ:
    case V_RINGS: case V_RINGTONE: case V_RINGEXC: case V_RINGDLY:
    case V_DAMBIENT: case V_RPROTOPT: case V_SPACE: case V_REVSEND: case V_REVMIX:
    case V_SETUP: case V_FM: return 1;
    default: return 0;
    }
}
static int wide_grid_page(int view) {
    return view==V_GARDEN || view==V_ROOTDIV || view==V_SETUP || view==V_SPACE || view==V_REVSEND || view==V_REVMIX ||
        view==V_PAD || view==V_PADBODY || view==V_PADSEQ ||
        view==V_DRONE || view==V_DRONEVOICE || view==V_PAD2SEQ ||
        view==V_RINGS || view==V_RINGTONE || view==V_RINGEXC ||
        view==V_RINGDLY || view==V_DAMBIENT || view==V_RUNITOPT;
}
static int grid_row(int v) { return ui.grid_row[v] == 2 ? 2 : 1; }
static int grid_binding(int view,int row,int slot)
{
    if(row==1)return VIEW_KNOB[view][slot];
    if(view==V_GARDEN)return slot==0 ? P_ROOT : K_NONE;
    if(view==V_RINGDLY) {
        static const int d[4]={P_DDIFF,P_DMOTION,P_DREV,K_NONE};
        return d[slot];
    }
    if(view==V_FM) {
        static const int f[4]={K_NONE,P_FMBURST,P_FMREV,K_NONE};
        return f[slot];
    }
    return K_NONE;
}
static const char *grid_label(int view,int row,int slot)
{
    if(row==1)return KNOB_LABEL[view][slot];
    if(view==V_GARDEN)return slot==0?"Root":"";
    if(view==V_RINGDLY){static const char *const d[]={"Diffusion","Motion","Rev Send",""};return d[slot];}
    if(view==V_FM){static const char *const f[]={"Density","Burst","Rev Send",""};return f[slot];}
    return "";
}
static int bound_view_knob(int view,int slot) {
    if(wide_grid_page(view))return VIEW_KNOB[view][slot];
    if(view==V_FMFILTER || view==V_PAD2FILTER)return fm_filter_knob(view,ui.fm_filter_row,slot);
    if(view==V_RUNITOPT || view==V_FUNITOPT)
        return ui.unit_row[view==V_FUNITOPT]==1?VIEW_KNOB[view][slot]:K_NONE;
    if(view==V_MOD)return mod_view_knob(ui.mod_row,slot);
    if(grid_page(view))return grid_binding(view,grid_row(view),slot);
    return VIEW_KNOB[view][slot];
}

static int mod_param(int pseudo)
{
    static const uint8_t A[11]={P_M1DEPTH,P_M2DEPTH,P_M3DEPTH,P_M4DEPTH,P_M5DEPTH,P_M6DEPTH,
                  P_M7DEPTH,P_M8DEPTH,P_M9DEPTH,P_M10DEPTH,P_M11DEPTH};
    static const uint8_t S[11]={P_M1SPEED,P_M2SPEED,P_M3SPEED,P_M4SPEED,P_M5SPEED,P_M6SPEED,
                  P_M7SPEED,P_M8SPEED,P_M9SPEED,P_M10SPEED,P_M11SPEED};
    static const uint8_t D[3]={P_M7DEST,P_M8DEST,P_M9DEST};
    static const uint8_t SH[5]={P_M7SHAPE,P_M8SHAPE,P_M9SHAPE,P_M10SHAPE,P_M11SHAPE};
    int r=ui.mod_route<11?ui.mod_route:0;
    if(pseudo==K_MOD_DEPTH)return A[r];
    if(pseudo==K_MOD_SPEED)return S[r];
    if(pseudo==K_MOD_SHAPE)return r>=6?SH[r-6]:-1;
    return r>=6&&r<9 ? D[r-6] : -1;
}

static int knob_get(int k)
{
    if (k >= K_PERF_MODE && k <= K_PERF_STATE) {
        perf_stats_t s; perf_read(&s);
        return k == K_PERF_MODE ? s.dual : k == K_PERF_SECS ? s.seconds : k == K_PERF_STATE ? s.status : 0;
    }
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_MOD_ROUTE) return ui.mod_route;
    if (k == K_MOD_DEST) { int p = mod_param(k); return p < 0 ? 0 : proj.par[p]; }
    if (k == K_MOD_DEPTH || k == K_MOD_SPEED || k == K_MOD_SHAPE) {int p=mod_param(k);return p<0?0:proj.par[p];}
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        if((k==P_FFILTERTYPE || k==P_P2FILTERTYPE) && (proj.par[k==P_P2FILTERTYPE?P_P2FILTERMODEL:P_FFILTERMODEL]==FM_FILTER_BYPASS || proj.par[k==P_P2FILTERTYPE?P_P2FILTERMODEL:P_FFILTERMODEL]>=FM_FILTER_JUNO)) {
            *lo=*hi=0;return;
        }
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
    } else if (k == K_MOD_ROUTE) {
        *lo = 0; *hi = 10;
    } else if (k == K_MOD_DEST) {
        int mp=mod_param(k); if(mp<0){*lo=0;*hi=0;} else {const en_param_t *p=en_param_info(mp);*lo=p->lo;*hi=p->hi;}
    } else if (k == K_MOD_DEPTH || k == K_MOD_SPEED || k == K_MOD_SHAPE) {
        int i=mod_param(k); if(i>=0){const en_param_t *p=en_param_info(i);*lo=p->lo;*hi=p->hi;}else{*lo=*hi=0;}
    } else {
        *lo = 0;
        *hi = k == K_LEDS ? 2 : k == K_CPU ? 100 : k == K_PERF_MODE ? 1 : 0;
    }
}
static const char *knob_name(int k)
{
    if (k < P_NPARAMS) return en_param_info(k)->name;
    if (k == K_MOD_ROUTE) return "Source";
    if (k == K_MOD_DEST) return "Destination";
    if (k == K_MOD_DEPTH) return "Depth";
    if (k == K_MOD_SPEED) return "Speed";
    if (k == K_MOD_SHAPE) return "Shape";
    if (k == K_PERF_MODE) return "Core mode";
    if (k == K_PERF_RESET) return "Reset meters";
    if (k == K_EVONEW || k == K_FMEVONEW) return "New pattern";
    if (k == K_RPROTNEW || k == K_FPROTNEW) return "New Proteus melody";
    return k == K_LEDS ? "Lights" : k == K_CPU ? "CPU" : "";
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k == K_EVONEW || k == K_FMEVONEW || k == K_RPROTNEW || k == K_FPROTNEW) { const char *t="Turn"; while((*b++=*t++)){}
    } else if (k == K_PERF_MODE || k == K_PERF_RESET || k == K_PERF_STATE) {
        const char *t = k == K_PERF_MODE ? (v ? "Dual" : "Single") : k == K_PERF_RESET ? "Turn" : v == 2 ? "Ready" : v == 3 ? "Fault" : "Off";
        if (k == K_PERF_STATE && v == 3) {
            switch (dc_fault()) {
            case DC_FAULT_TIMEOUT: t = "Timeout"; break;
            case DC_FAULT_MAILBOX: t = "Check"; break;
            case DC_FAULT_STACK: t = "Stack"; break;
            default: break;
            }
        }
        while ((*b++ = *t++)) {};
    } else if (k == K_PERF_SECS) { itoa_u((uint32_t)v,b);
    } else if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_MOD_ROUTE) {
        b[0]='M';if(v<9){b[1]=(char)('1'+v);b[2]=0;}
        else{b[1]='1';b[2]=(char)('0'+v-9);b[3]=0;}
    } else if (k == K_MOD_DEST) {
        int mp=mod_param(k);
        if(ui.mod_route>=9) {const char *x=ui.mod_route==9?"Pad Cut":"Pad Res";while((*b++=*x++));}
        else if(mp<0){ const char *x="Pad fixed"; while((*b++=*x++)); } else en_param_text(mp,v,b);
    } else if (k == K_MOD_DEPTH || k == K_MOD_SPEED || k == K_MOD_SHAPE) {
        int mp=mod_param(k);if(mp>=0)en_param_text(mp,v,b);else if(k==K_MOD_SHAPE && ui.mod_route<6){const char *x="Smooth Rnd";while((*b++=*x++)){} }else b[0]=0;
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else {
        b[0] = 0;
    }
}

/* Single-page 2x4 control map for Original, Hallucigenia (formerly Evolve)
 * and Proteus. Mode is selected by Knob 1 only while SELECT highlights MODE. */
static int seq_binding(int view, int zone, int slot)
{
    int fm = view == V_FMSEQ;
    int mode = proj.par[fm ? P_FMSEQMODE : P_RSEQMODE];
    if (zone == 0) return slot == 0 ? (fm ? P_FMSEQMODE : P_RSEQMODE) : K_NONE;
    if (mode == 3) {
        static const int u[2][8] = {
            {P_RUDSEED,P_RUDNODES,P_RUDRADIUS,P_RUDDENS,
             P_RUDWALK,P_RUDLOCK,P_RUDGATE,P_RUDGDEN},
            {P_FUDSEED,P_FUDNODES,P_FUDRADIUS,P_FUDDENS,
             P_FUDWALK,P_FUDLOCK,P_FUDGATE,P_FUDGDEN}
        };
        return u[fm][(zone-1)*4+slot];
    }
    if (mode == 2) {
        static const int r[2][8] = {
            {P_RPROTLEN,P_RPROTDENS,P_RPROTPATIENCE,P_RPROTRATE,
             P_RPROTGATE,P_RPROTOCT,P_RPROTMUTATE,P_RPROTLOCK},
            {P_FPROTLEN,P_FPROTDENS,P_FPROTPATIENCE,P_FPROTRATE,
             P_FPROTGATE,P_FPROTOCT,P_FPROTMUTATE,P_FPROTLOCK}
        };
        return r[fm][(zone-1)*4+slot];
    }
    if (mode == 1) {
        static const int r[2][8] = {
            {P_ELENGTH,P_EMUTATE,P_EDENSITY,P_ERATE,P_ERANGE,P_EFOLLOW,P_EPATTERN,K_EVONEW},
            {P_FMELENGTH,P_FMEMUTATE,P_FMEDENSITY,P_FMERATE,P_FMERANGE,P_FMEFOLLOW,P_FMEPATTERN,K_FMEVONEW}
        };
        return r[fm][(zone-1)*4+slot];
    }
    /* Original keeps its existing loop, octave and Burst controls. */
    static const int r[2][8] = {
        {P_BOCT,P_RINGRANGE,P_RINGCHANCE,P_RBURST,K_NONE,P_BELLS,P_RMODEL,P_RPOLY},
        {P_FMOCT,P_FMRANGE,P_FMCHANCE,P_FMBURST,K_NONE,P_FMLEVEL,P_FMPATCH,P_FMPOLY}
    };
    return r[fm][(zone-1)*4+slot];
}
static const char *seq_binding_label(int view, int zone, int slot)
{
    if (zone == 0) return slot==0 ? "Mode" : "";
    static const char *const ud[] = {"Seed","Nodes","Radius","Density","Walk","Lock","Gate Len","Gate Den"};
    static const char *const prot[] = {"Length","Density","Patience","Rate",
                                       "Gate","Oct Prob","Mutate","Lock"};
    static const char *const hall[] = {"Length","Mutation","Density","Rate",
                                       "Range","Follow","Pattern","New"};
    static const char *const orig[] = {"Octave","Oct Drift","Chance","Burst",
                                       "Density","Level","Model","Voices"};
    int fm=view==V_FMSEQ;
    int mode=proj.par[fm ? P_FMSEQMODE : P_RSEQMODE];
    return (mode==3?ud:mode==2?prot:mode==1?hall:orig)[(zone-1)*4+slot];
}

static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE) return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k)) return;
    if (k == K_PERF_MODE) { perf_mode(v); return; }
    if (k == K_MOD_ROUTE) {
        ui.mod_route = (uint8_t)v;
        ui.mod_row=1;
        ui.sig[1]=ui.sig[2]=0;
    } else if (k == K_MOD_DEST) {
        int p=mod_param(k); if(p>=0){proj.par[p]=(int16_t)v; en_set(p,v);}
    } else if (k == K_MOD_DEPTH || k == K_MOD_SPEED || k == K_MOD_SHAPE) {
        int p=mod_param(k);if(p>=0){proj.par[p]=(int16_t)v;en_set(p,v);}
    } else if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v; en_set(k, v);
        if(k==P_FFILTERMODEL || k==P_P2FILTERMODEL) {
            int type=0;
            int param=k==P_P2FILTERMODEL?P_P2FILTERTYPE:P_FFILTERTYPE;proj.par[param]=(int16_t)type;en_set(param,type);
        }
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    }
    mark_dirty();
}

/* knob acceleration by speed (X0X's): a few clicks are one step each however quick, so small
 * corrections stay exact; a longer quick turn speeds up to 8 a detent, so half a turn sweeps 0-100.
 * Small ranges never accelerate. */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;                                      /* per detent */
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
    /* The simulator's documented spin command bypasses acceleration. */
    if (plat_host_exact) return s;
#endif
    if (range <= 24 || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (e && k == K_EVONEW) { en_evo_new(); mark_dirty(); say("PATTERN", "Randomized"); return; }
    if (e && k == K_FMEVONEW) { en_fmevo_new(); mark_dirty(); say("FM PATTERN", "Randomized"); return; }
    if (e && (k == K_RPROTNEW || k == K_FPROTNEW)) {
        en_proteus_new(k==K_FPROTNEW);
        mark_dirty();say("PROTEUS", "NEW MELODY");return;
    }
    if (e && k == K_PERF_RESET) { perf_reset(); return; }
    if (k == K_PERF_SECS || k == K_PERF_STATE) return;
    if (!e || k == K_NONE || k == K_CPU)
        return;
    if (k == P_RMODEL) {
        /* Supported choices only; saved model IDs keep their original numbers. */
        int index = rings_model_index(knob_get(k));
        index += accel(role, e, RINGS_MODEL_COUNT - 1);
        knob_set(k, rings_model_from_index(index));
        return;
    }
    if(k==P_ERATE || k==P_FMERATE || k==P_RPROTRATE || k==P_FPROTRATE || k==P_RUDRATE || k==P_FUDRATE) {
        knob_set(k,seq_rate_from_index(seq_rate_index(knob_get(k)) + e));return;
    }
    knob_range(k, &lo, &hi);
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ------------------------------------------------------------- the pool --- */
static void toggle_note(int pc)                 /* a key: its note in or out of the scale */
{
    int rel = (pc - proj.par[P_ROOT] + 12) % 12;
    uint32_t p = proj.pool ^ (1u << rel);
    if (rel == 0) {
        warn("THE ROOT STAYS", "(HOME)");
        return;
    }
    if (pool_count(p) < 3) {
        warn("3 NOTES", "AT LEAST");
        return;
    }
    proj.pool = (uint16_t)p;
    en_pool(p);
    say(p >> rel & 1u ? "ADDED" : "REMOVED", EN_NOTE_NAME[pc]);
    mark_dirty();
}

static void set_mode(int m)
{
    m = (m % EN_NMODES + EN_NMODES) % EN_NMODES;
    if (m != MODE_PIANO) fm_manual_release();
    proj.par[P_MODE] = (int16_t)m;
    en_set(P_MODE, m);
    if (m != MODE_PIANO) {
        proj.pool = EN_MODE_POOL[m];
        en_pool(proj.pool);
    }
    /* PIANO deliberately keeps the previous scale pool for Pad 1/Pad 2; only
     * the keys play FM on its pages and Rings on the other pages. */
    say("SCALE", EN_MODE_NAME[m]);
    mark_dirty();
}

static void set_piece(int i)                    /* the piece playing kept as it is; piece i as it was left */
{
    fm_manual_release();
    project_piece(i);
    project_apply();
    en_seed(plat_ms());
    en_regrow();
    say_as("PIECE", piece_name(proj.piece), 0);
    sp_piece();                                  /* a screen reader: the piece with its key and tempo */
    mark_dirty();
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    ui.view = (uint8_t)v;
    if(v==V_FMFILTER || v==V_PAD2FILTER)ui.fm_filter_row=1;
    if(v==V_RINGSEQ || v==V_FMSEQ)ui.seq_zone[v==V_FMSEQ]=1;
    ui.touched = -1;
}

static void button(int b)
{
    int v;
    /* Custom page-button layout: ENV=Pad 1, ARP=Pad 2, SEQ=Rings,
     * FX=Reverb, GLO=Mixer. Setup remains available as the second
     * GLO page so no existing configuration page is lost. */
    if (b == B_HOME) { set_view(ui.view==V_GARDEN?V_ROOTDIV:V_GARDEN); return; }
    if (b == B_ENV) { set_view(ui.view == V_PAD ? V_PADBODY : ui.view == V_PADBODY ? V_FMFILTER : ui.view == V_FMFILTER ? V_PADSEQ : V_PAD); return; }
    if (b == B_ARP) { set_view(ui.view == V_DRONE ? V_DRONEVOICE : ui.view == V_DRONEVOICE ? V_PAD2FILTER : ui.view == V_PAD2FILTER ? V_PAD2SEQ : V_DRONE); return; }
    if (b == B_SEQ) {
        set_view(ui.view == V_RINGS ? V_RINGTONE : ui.view == V_RINGTONE ? V_RINGEXC :
                 ui.view == V_RINGEXC ? V_RINGDLY : ui.view == V_RINGDLY ? V_DAMBIENT :
                 ui.view == V_DAMBIENT ? V_RINGSEQ :
                 ui.view == V_RINGSEQ && proj.par[P_RSEQMODE] == 2 ? V_RPROTOPT :
                 ui.view == V_RINGSEQ && proj.par[P_RSEQMODE] == 3 ? V_RUNITOPT : V_RINGS);
        return;
    }
    if (b == B_SEL) {
        set_view(ui.view == V_FM ? V_FMSEQ :
                 ui.view == V_FMSEQ && proj.par[P_FMSEQMODE] == 2 ? V_FPROTOPT :
                 ui.view == V_FMSEQ && proj.par[P_FMSEQMODE] == 3 ? V_FUNITOPT : V_FM);
        return;
    }
    if (b == B_FX) { set_view(ui.view == V_SPACE ? V_REVSEND : ui.view == V_REVSEND ? V_SPACE : V_SPACE); return; }
    if (b == B_GLO) { set_view(ui.view == V_MIXER ? V_SETUP : ui.view == V_SETUP ? V_PERF : V_MIXER); return; }
    for (v = 0; v < NVIEWS; v++)                 /* a page's button: its page */
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_EDIT:
        en_next();
        say("NEXT CHORD", en_playing ? "ON THE BAR" : 0);
        break;
    case B_PLAY:
        sp_tp.playing = !en_playing;
        sp_tp.play_t = plat_ms();
        if (en_playing) {
            en_play(0);
            say("RESTING", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("GROWING", 0);
        }
        break;
    case B_REC:
        say("FREEZE", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_OCTDN: case B_OCTUP:
        {
            int p;
            const char *name;
            if (ui.view == V_PAD || ui.view == V_PADBODY || ui.view == V_PADSEQ || ui.view==V_FMFILTER) { p = P_PADOCT; name = "PAD OCTAVE"; }
            else if (ui.view == V_DRONE || ui.view == V_DRONEVOICE || ui.view == V_PAD2SEQ || ui.view==V_PAD2FILTER) { p = P_PAD2OCT; name = "PAD 2 OCTAVE"; }
            else if (fm_view(ui.view)) { p = P_FMOCT; name = "FM OCTAVE"; }
            else { p = P_BOCT; name = "RINGS OCTAVE"; }
            char t[8];
            knob_set(p, proj.par[p] + (b == B_OCTUP ? 1 : -1));
            en_param_text(p, proj.par[p], t);
            say(name, t);
            ui.msg_until = plat_ms() + 2500u;
        }
        break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            warn("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* Channel 1 keeps the scale/Rings role. Channel 3 plays FM in PIANO mode.
 * FM releases are honored after mode, octave or page changes as well. */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt, 0)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = (pkt >> 24) & 0x7Fu;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if (st == 0x82u || (st == 0x92u && !v)) {
            if (fm_midi_note[n]) {
                en_fm_note_off(fm_midi_note[n] - 1); fm_midi_note[n] = 0;
            }
        } else if (st == 0x92u && v && proj.par[P_MODE] == MODE_PIANO) {
            int note = fm_clip_note((int)n + 12 * proj.par[P_FMOCT]);
            if (fm_midi_note[n]) en_fm_note_off(fm_midi_note[n] - 1);
            fm_midi_note[n] = (uint8_t)(note + 1);
            en_fm_note(note, (int)v);
        } else if (st == 0xB2u && (n == 120u || n == 123u)) {
            en_fm_all_notes_off();
            memset(fm_key_note, 0, sizeof fm_key_note);
            memset(fm_midi_note, 0, sizeof fm_midi_note);
        } else if (st == 0x90u && v) {
            if (proj.par[P_MODE] == MODE_PIANO)
                en_rings_note((int)n + 12 * proj.par[P_BOCT], (int)v);
            else
                toggle_note((int)(n % 12u));
        }
    }
}

/* HELP CARDS (X0X's): hold one button alone for HELP_HOLD and a card says what it does. A key, a
 * turn or another button puts it away; letting go after its card does nothing else. PLAY acts on
 * its press and has no card. SEL opens the FM pages. */
#define HELP_HOLD 900u
#define RESET_HOLD 1500u
static void help_tick(uint32_t btn, uint32_t keys)
{
    uint32_t now = plat_ms();
    int i;
    if (ui.help) {
        uint32_t m = 1u << (ui.help - 1);
        if (btn != m || keys || (int32_t)(ui.turn_t - ui.down_t[ui.help - 1]) > 0)
            ui.help = 0;
        return;
    }
    if (!btn || (btn & (btn - 1u)) || keys)
        return;
    for (i = 0; i < NB; i++)
        if (btn == 1u << i && i != B_PLAY && !(ui.btn_used & btn) && now - ui.down_t[i] >= HELP_HOLD &&
            (int32_t)(ui.turn_t - ui.down_t[i]) <= 0) {
            ui.help = (uint8_t)(i + 1);
            ui.btn_used |= btn;                  /* its release now does nothing */
        }
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (i == B_SAVE)
                ui.reset_done = 0;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++) {
        if ((ch & ~keys) >> i & 1u) {
            if (fm_key_note[i]) {
                en_fm_note_off(fm_key_note[i] - 1); fm_key_note[i] = 0;
            }
        } else if ((ch & keys) >> i & 1u) {
            if (proj.par[P_MODE] == MODE_PIANO) {
                /* FM-1 key 0 is F3 (MIDI 53), over the full 27-key range. */
                if (fm_view(ui.view)) {
                    int note = fm_clip_note(53 + (int)i + 12 * proj.par[P_FMOCT]);
                    fm_key_note[i] = (uint8_t)(note + 1);
                    en_fm_note(note, 100);
                } else {
                    en_rings_note(53 + (int)i + 12 * proj.par[P_BOCT], 100);
                }
            } else {
                toggle_note((int)((5u + i) % 12u));
            }
        }
    }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    {   /* MASTER: show the live physical value in the same one-line header used by button confirmations.
         * Ignore the initial boot position; thereafter a one-percent change refreshes the popup. */
        uint32_t m = plat_master();
        uint32_t pct = (m * 100u + 2048u) / 4096u;
        if (pct > 100u) pct = 100u;
        if (!ui.master_seen) {
            ui.master_seen = 1;
            ui.master_pct = (uint8_t)pct;
        } else if ((uint8_t)pct != ui.master_pct) {
            char t[8], *q;
            ui.master_pct = (uint8_t)pct;
            itoa_u(pct, t);
            q = t; while (*q) q++; *q++ = '%'; *q = 0;
            say_as("MASTER", t, 0);
            ui.act_t = plat_ms();
        }
    }
    if ((e = plat_enc(EN_SELECT)) != 0) {
        if(ui.view==V_FMFILTER || ui.view==V_PAD2FILTER) {
            ui.fm_filter_row=(uint8_t)(((int)(ui.fm_filter_row?ui.fm_filter_row:1)-1+e%2+2)%2+1);
            ui.sig[1]=ui.sig[2]=0;
            say("SELECT",ui.fm_filter_row==1?"ROW 1":"ROW 2 MODEL");
        } else if(ui.view==V_MOD && ui.mod_route<11) {
            ui.mod_row=(uint8_t)(((int)(ui.mod_row?ui.mod_row:1)-1+e%2+2)%2+1);
            ui.sig[1]=ui.sig[2]=0;
            say("SELECT",ui.mod_row==1?"ROW 1":"ROW 2 SHAPE");
        } else if(ui.view == V_FUNITOPT) {
            int fi=ui.view==V_FUNITOPT;
            int row=(int)(ui.unit_row[fi]?ui.unit_row[fi]:1)+e-1;
            while(row<0)row+=2;
            ui.unit_row[fi]=(uint8_t)(row%2+1);
            ui.sig[1]=ui.sig[2]=0;
            say("SELECT",ui.unit_row[fi]==1?"ROW 1":"ROW 2 RESERVED");
        } else if (ui.view == V_RINGSEQ || ui.view == V_FMSEQ) {
            int fi=ui.view==V_FMSEQ;
            int zone=(int)(ui.seq_zone[fi]==2?2:1)-1+e;
            while(zone<0)zone+=2;
            ui.seq_zone[fi]=(uint8_t)(zone%2+1);
            ui.sig[0]=ui.sig[1]=ui.sig[2]=0;
            say("SELECT",ui.seq_zone[fi]==1?"BANK 1":"BANK 2");
        } else if (ui.view == V_GARDEN || ui.view == V_ROOTDIV) {
            if (btn >> B_HOME & 1u) {
                set_view(ui.view==V_GARDEN?V_ROOTDIV:V_GARDEN);
                ui.btn_used|=1u<<B_HOME;ui.sig[0]=ui.sig[1]=ui.sig[2]=0;
            } else {
                int old=proj.par[P_ROOT];turn(EN_SELECT,P_ROOT,e);
                if(proj.par[P_ROOT]!=old)say("ROOT",EN_NOTE_NAME[proj.par[P_ROOT]]);
            }
        } else if(grid_page(ui.view) && !wide_grid_page(ui.view)) {
            int row=grid_row(ui.view)-1+e;
            while(row<0)row+=2;
            ui.grid_row[ui.view]=(uint8_t)(row%2+1);
            ui.sig[1]=ui.sig[2]=0;
            say("SELECT",grid_row(ui.view)==1?"ROW 1":"ROW 2");
        }
        /* SELECT is intentionally not a global Root encoder. */
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        if(ui.view==V_RINGSEQ || ui.view==V_FMSEQ){
            int fi=ui.view==V_FMSEQ,k=fi?P_FMSEQMODE:P_RSEQMODE;
            int mode=(proj.par[k]+e%4+4)%4;
            knob_set(k,mode);ui.seq_zone[fi]=1;
            ui.sig[0]=ui.sig[1]=ui.sig[2]=0;
            char value[24];knob_text(k,value);say("SEQUENCER",value);
        }else set_mode(proj.par[P_MODE] + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        set_piece(proj.piece + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((btn >> B_SAVE & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_SAVE] > RESET_HOLD) {
        ui.reset_done = 1;                     /* held: the piece back to its recipe (not saved) */
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        fm_manual_release();
        project_piece_reset();
        project_apply();
        en_regrow();
        say("RESET", piece_name(proj.piece));
        mark_dirty();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            int k = (ui.view==V_RINGSEQ || ui.view==V_FMSEQ) ?
                seq_binding(ui.view,ui.seq_zone[ui.view==V_FMSEQ],(int)i) :
                bound_view_knob(ui.view,(int)i);
            turn(EN_K1 + (int)i,k,e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
    help_tick(btn, keys);
}

/* ------------------------------------------------------------- screen --- */
/* Four-row readable UI: a page label, three static display rows, then the active/editable row. */
#define HDR_H 36
#define MAIN_Y 36
#define MAIN_H 132
#define KNB_Y 168
#define KNB_H 72

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

/* a filled circle of radius r (0..6) centred on (cx, cy): HW[r][d] is the half width d rows from the middle */
static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    static const uint8_t HW[7][7] = {{0}, {1, 0}, {2, 1, 0}, {3, 2, 2, 0}, {4, 4, 3, 2, 0}, {5, 5, 4, 4, 3, 0},
                                     {6, 6, 5, 5, 4, 3, 0}};
    int32_t j;
    if (r <= 0) {
        cv_rect(cx, cy, 1, 1, c);
        return;
    }
    if (r > 6)
        r = 6;
    for (j = -r + 1; j < r; j++) {
        int32_t w = HW[r][j < 0 ? -j : j];
        cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

/* anti-aliased drawing (X0X's): a pixel blended over the canvas, a disc, and a knob's ring */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* a knob: a 270-degree track from 7:30 to 4:30, anti-aliased, round ends; the value filled from
 * the start (or from the top, for a centred value) and a round handle */
static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    const float th = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
    }
}

static void caps(char *d, const char *s, int n)  /* s in capitals, at most n - 1 characters */
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }

static void tag(int32_t *x, const char *s, uint16_t bg, uint16_t fg)
{
    int32_t w = text_w(&FONT_XS, s) + 10;
    cv_round(*x, 6, w, 16, 6, bg);
    cv_text(*x + 5, 7, &FONT_XS, s, fg);
    *x += w + 4;
}

/* the header: the piece in a tag, its root and mode; on the right, the tempo with a slow beat light
 * (a play triangle while resting) and a dot while there is something unsaved. Messages take its place
 * for a moment. */
static uint16_t page_colour(int v)
{
    if (v == V_PAD || v == V_PADBODY || v == V_PADSEQ) return K_PAD;
    if (v == V_RUNITOPT) return K_BELL;
    if (v == V_RINGS || v == V_RINGTONE || v == V_RINGEXC || v == V_RINGDLY || v == V_DAMBIENT || v == V_RINGSEQ || v == V_ESETUP || v == V_EPATTERN || v == V_RPROTOPT) return K_BELL;
    if (fm_view(v)) return K_FM;
    if (v == V_MOD) return RGB(40, 224, 112);
    if (v == V_MIXER) return K_ACC;
    if (v == V_DRONE || v == V_DRONEVOICE || v == V_PAD2SEQ) return K_PULSE;
    if (v == V_SPACE || v == V_REVSEND || v == V_REVMIX) return K_ACC;
    return K_ACC;
}

static void outline_box(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t edge, uint16_t fill)
{
    cv_round(x, y, w, h, 3, edge);
    if (w > 2 && h > 2) cv_round(x + 1, y + 1, w - 2, h - 2, 2, fill);
}

static const felucca_font_t *fit_value_font(const char *t, int32_t w)
{
    if (text_w(&FONT_B, t) <= w - 6) return &FONT_B;
    if (text_w(&FONT_S, t) <= w - 6) return &FONT_S;
    return &FONT_XS;
}

/* Shared panel typography: caption, value and knob occupy separate bands. */
static const felucca_font_t *panel_fit(const char *text,int width,int height) {
    const felucca_font_t *fonts[]={&FONT_M,&FONT_B,&FONT_S,&FONT_XS,&FONT_T};
    for(int i=0;i<5;i++)if(fonts[i]->h<=height && text_w(fonts[i],text)<=width)return fonts[i];
    return &FONT_T;
}
static void panel_text(int x,int y,int w,int h,const char *label,const char *value,const char *key,uint16_t col,uint16_t labelcol) {
    char first[32],second[32];int split=-1,n=0;
    while(value[n] && n<31){if(value[n]==' ')split=n;n++;}
    if(split>0 && value[split+1]) {
        int i;for(i=0;i<split;i++)first[i]=value[i];first[i]=0;
        for(i=split+1;value[i] && i-split-1<31;i++)second[i-split-1]=value[i];
        second[i-split-1]=0;
        const felucca_font_t *f=panel_fit(first,w-10,(h-16)/2);
        const felucca_font_t *g=panel_fit(second,w-10,(h-16)/2);
        if(g->h<f->h)f=g;
        int keyw=text_w(&FONT_T,key),labelw=w-12-keyw;
        const felucca_font_t *lf=panel_fit(label,labelw,12);
        if(2*f->h<=h-16 && text_w(f,first)<=w-10 && text_w(f,second)<=w-10 && text_w(lf,label)<=labelw) {
            cv_text(x+4,y+2,lf,label,labelcol);
            cv_text(x+w-4-keyw,y+2,&FONT_T,key,col);
            int top=y+14+(h-16-2*f->h)/2;
            text_c(x+w/2,top,f,first,K_TEXT);text_c(x+w/2,top+f->h,f,second,K_TEXT);
            return;
        }
    }
    const felucca_font_t *lf=text_w(&FONT_XS,label)<=w-8?&FONT_XS:&FONT_T;
    int top=y+3+lf->h+1,foot=y+h-3-FONT_T.h;
    const felucca_font_t *vf=panel_fit(value,w-10,foot-top-1);
    cv_text(x+4,y+3,lf,label,labelcol);
    text_c(x+w/2,top+(foot-top-1-vf->h)/2,vf,value,K_TEXT);
    text_c(x+w/2,foot,&FONT_T,key,col);
}

static void value_box(int32_t x, int32_t y, int32_t w, int32_t h, const char *label, const char *value,
                      uint16_t edge, uint16_t fill, int hot)
{
    const felucca_font_t *f;
    uint16_t bg = hot ? mix(fill, edge, 4) : fill;
    outline_box(x, y, w, h, edge, bg);
    const felucca_font_t *lf=panel_fit(label,w-8,h<34?12:14);
    cv_text(x+4,y+2,lf,label,hot?edge:K_DIM);
    int top=y+3+lf->h,space=h-(top-y)-3;
    f=panel_fit(value,w-10,space);
    text_c(x+w/2,top+(space-f->h)/2,f,value,hot?edge:K_TEXT);
}

/* Approval preview: four Rings secondary pages share an enlarged parameter grid
 * in the space freed by removing the mixer. This is UI-only: the existing
 * physical encoder indices and sound parameters do not change.
 * Coordinates are relative to MAIN_Y; the final 72 rows are drawn in the
 * separate control band, using cv_oy to keep a seamless 240x240 screen. */
static int rings_aux_grid_page(void)
{
    return ui.view == V_RINGTONE || ui.view == V_RINGEXC ||
           ui.view == V_RINGDLY || ui.view == V_DAMBIENT;
}

static void rings_aux_grid_box(int x, int y, int w, int h, int slot, uint16_t colour)
{
    char value[24];
    int k = VIEW_KNOB[ui.view][slot];
    int hot = plat_ms() < ui.touch_until && ui.touched == slot;
    const felucca_font_t *f;
    if (k == K_NONE) return;
    knob_text(k, value);
    outline_box(x,y,w,h,colour, hot ? mix(K_PANEL, colour, 4) : K_PANEL);
    cv_text(x+5,y+5,&FONT_XS,KNOB_LABEL[ui.view][slot],hot ? colour : K_DIM);
    f = fit_value_font(value,w);
    /* Keep the value centred in its generous box; don't clip long values. */
    text_c(x+w/2, y+(h>100 ? h/2+8 : h-22), f, value, hot ? colour : K_TEXT);
}

static void draw_rings_aux_grid(void)
{
    int slots[4], n=0;
    uint16_t c = page_colour(ui.view);
    for(int i=0;i<4;i++) if(VIEW_KNOB[ui.view][i]!=K_NONE) slots[n++]=i;
    outline_box(1,65,238,139,c,K_BG);
    if(n==2) {
        /* Tone has two controls; fill the available height with two columns. */
        rings_aux_grid_box(4,68,114,132,slots[0],c);
        rings_aux_grid_box(122,68,114,132,slots[1],c);
    } else {
        rings_aux_grid_box(4,68,114,65,slots[0],c);
        rings_aux_grid_box(122,68,114,65,slots[1],c);
        if(n==3) {
            /* Three controls: the last box spans the entire second row. */
            rings_aux_grid_box(4,136,232,65,slots[2],c);
        } else if(n==4) {
            rings_aux_grid_box(4,136,114,65,slots[2],c);
            rings_aux_grid_box(122,136,114,65,slots[3],c);
        }
    }
}

/* Always-visible page mapping label. */
static void draw_header(void)
{
    char page[24], battery[8];
    const felucca_font_t *pf;
    uint16_t c = page_colour(ui.view);
    int32_t x, bp = plat_battery_pct();
    cv_begin(240, HDR_H, K_BG);
    /* Two-part info panel: current page at left, live battery estimate in the
     * former BUTTON field at right. Button/parameter confirmations temporarily
     * overlay the panel as one-line messages. */
    outline_box(1, 1, 238, 34, c, K_PANEL);
    if (plat_ms() < ui.msg_until) {
        text_c(120, 11, &FONT_S, ui.msg, c);
        cv_blit(0, 0);
        return;
    }
    caps(page, VIEW_NAME[ui.view], sizeof page);
    if (bp < 0) {
        battery[0] = '-'; battery[1] = '-'; battery[2] = 0;
    } else if (bp > 100) {
        battery[0] = 'U'; battery[1] = 'S'; battery[2] = 'B'; battery[3] = 0;
    } else {
        char *b = battery;
        itoa_u((uint32_t)bp, b);
        while (*b) b++;
        *b++ = '%'; *b = 0;
    }
    cv_text(6, 2, &FONT_XS, dc_status() == 2 ? "PAGE 2C" : dc_status() == 3 ? "PAGE C2!" : dc_stage() == 1 ? "PAGE 1C S1" : dc_stage() == 2 ? "PAGE 1C S2" : dc_stage() == 3 ? "PAGE 1C S3" : "PAGE 1C S0", K_DIM);
    cv_text(178, 2, &FONT_XS, "BATTERY", K_DIM);
    pf = text_w(&FONT_M, page) <= 154 ? &FONT_M : text_w(&FONT_S, page) <= 154 ? &FONT_S : &FONT_XS;
    cv_text(6, 12, pf, page, c);
    x = 235 - text_w(&FONT_M, battery);
    cv_text(x, 12, &FONT_M, battery, K_TEXT);
    cv_blit(0, 0);
}


/* the garden: the scale as a ring of twelve, the chord lit in it; the four bell loops around it, each
 * a ring of its steps with its notes on it and its playhead going round; the three pulses on the
 * right, the four layers' levels on the left */
#define CX 120
#define CY 84
static const int32_t LOOP_R[EN_NLOOP] = {36, 43, 50, 57};

static void polar(int32_t cx, int32_t cy, float r, int i, int n, int32_t *x, int32_t *y)
{
    float a = (float)i * (FM_TWO_PI / (float)n);  /* step i of n, clockwise from the top */
    float sx = fm_sinf(a) * r, sy = fm_cosf(a) * r;
    *x = cx + (int32_t)(sx + (sx < 0.0f ? -0.5f : 0.5f));
    *y = cy - (int32_t)(sy + (sy < 0.0f ? -0.5f : 0.5f));
}

static const char *chord_now(char *nm)          /* the chord's name, as the garden shows it ("Em7") */
{
    int pcs[EN_NPAD + 1], k;
    pcs[0] = en_chord_root;
    for (k = 0; k < EN_NPAD; k++)
        pcs[k + 1] = en_chord[k] % 12;
    return en_chord_name(pcs, EN_NPAD + 1, nm);
}

static void draw_ring(void)
{
    uint32_t abs_pool = pool_abs(), chord = 0;
    int i, l;
    for (i = 0; i < EN_NPAD; i++)
        chord |= 1u << (en_chord[i] % 12u);
    for (l = 0; l < EN_NLOOP; l++) {
        int len = EN_LOOP_LEN[l], s, pos = en_loop_pos[l], g = q16(en_loop_glow[l]);
        for (s = 0; s < len; s++) {
            int32_t x, y;
            polar(CX, CY, (float)LOOP_R[l], s, len, &x, &y);
            if (en_loop_note[l][s] >= 0)
                cv_disc(x, y, 2, (s == pos && en_playing) ? mix(K_BELL, K_FROST, g / 2) : mix(K_BELL_T, K_BELL, 12));
            else
                cv_rect(x, y, 2, 2, K_LINE);
        }
        if (en_playing) {                        /* the playhead */
            int32_t x, y;
            polar(CX, CY, (float)LOOP_R[l], pos, len, &x, &y);
            cv_disc(x, y, 1 + g / 6, mix(K_DIM, K_BELL, g));
        }
    }
    for (i = 0; i < 12; i++) {                  /* the scale: C at the top */
        int32_t x, y;
        int in = (int)(abs_pool >> i & 1u), ch = (int)(chord >> i & 1u);
        polar(CX, CY, 24.0f, i, 12, &x, &y);
        if (i == proj.par[P_ROOT])
            cv_disc(x, y, 5, K_TEXT);           /* the root: ringed in ink */
        if (ch)
            cv_disc(x, y, 4, mix(K_PAD_T, K_PAD, 6 + q16(en_pad_level) * 10 / 16));
        else if (in)
            cv_disc(x, y, 3, mix(K_PAD_T, K_PAD, 7));
        else
            cv_disc(x, y, 1, K_LINE);
    }
    {   /* the chord's name, in the middle */
        char nm[16];
        chord_now(nm);
        if (text_w(&FONT_B, nm) <= 38)
            text_c(CX, CY - 8, &FONT_B, nm, en_frozen ? K_FROST : K_TEXT);
        else
            text_c(CX, CY - 6, &FONT_XS, nm, en_frozen ? K_FROST : K_TEXT);
    }
}

static void draw_pulse(void)                     /* right: independent half-speed Pad 2 */
{
    int i;
    int32_t cx = 212, cy = 86;
    float lev = fm_clampf(en_drone_level, 0.0f, 1.0f);
    int g = q16(lev);
    for (i = 0; i < 3; i++) {
        int32_t y = cy + (i - 1) * 27;
        int32_t r = 5 + (g * (i == 1 ? 5 : 4)) / 16;
        cv_disc(cx, y, r, mix(K_PULSE_T, K_PULSE, 6 + g / 2));
        cv_disc(cx, y, 2, K_PANEL);
    }
    if (g)
        cv_round(cx - 20, cy - 43, 40, 86, 6, mix(K_LINE, K_PULSE_T, g / 2));
}

/* a button's card (help_tick): its name, then what it does, in the garden's place. A line starting with a
 * space goes on from the one before (a screen reader joins them: sp_line) */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"REVERB", "FM", "PAD 1", "LFO", "EDIT", "MIXER", "HOME", "SAVE", "PAD 2", "RINGS",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("THE GARDEN"); L("TEMPO, EVOLVE, MOTION, DRIFT"); L("SELECT: ROOT (HOME ONLY)"); L("HOLD HOME + SELECT: ROW"); break;
    case B_ENV: L("PAD 1"); L("AGAIN: BODY / FILTER / SEQ"); L("LEVEL, OCTAVE, SWELL, BRIGHT"); break;
    case B_ARP: L("PAD 2 - INDEPENDENT"); L("AGAIN: BODY / FILTER / SEQ"); L("HALF-SPEED PAD ENGINE"); break;
    case B_LFO: L("MODULATION"); L("SOURCE, DESTINATION, DEPTH, SPEED"); break;
    case B_SEQ: L("RINGS"); L("TONE / EXCITER / DELAY"); L("DELAY 2: DIFFUSION / MOTION / SEND"); L("SEQ: ALGO MODE / SELECT BANK"); break;
    case B_SEL: L("FM - SIX OPERATORS"); L("AGAIN: SEQUENCER"); L("SEQ: ALGO MODE / SELECT BANK"); L("PIANO: KEYS OR MIDI CH 3"); break;
    case B_FX: L("REVERB"); L("MONUMATIC FULL-RATE REVERB"); L("PRESS AGAIN: LAYER SENDS"); break;
    case B_GLO: L("MIXER"); L("PAD, RINGS, PAD 2, FM"); L("PRESS AGAIN: SETUP"); L("THEN PERFORMANCE"); break;
    case B_EDIT: L("THE NEXT CHORD,"); L("ON THE NEXT BAR"); break;
    case B_REC: L("FREEZE: THE CHORD HOLDS"); L(" AND THE REVERB RINGS ON"); L("AGAIN: THAW"); break;
    case B_OCTDN: L("CURRENT LAYER OCT DOWN"); L("FOLLOWS THE CURRENT PAGE"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP: L("CURRENT LAYER OCT UP"); L("FOLLOWS THE CURRENT PAGE"); L("WITH OCT-, 5 S: UPDATE MODE"); break;
    case B_SAVE: L("SAVES ALL EIGHT PIECES"); L("KEEP HOLDING: THE PIECE"); L(" BACK TO ITS RECIPE"); break;
    default: break;
    }
#undef L
    return n;
}

static void draw_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    cv_round(16, 14, 208, 116, 6, K_PANEL);
    cv_round(16, 14, 208, 26, 6, K_TEXT);
    cv_rect(16, 34, 208, 6, K_TEXT);
    {
        const char *bn = BTN_LABEL[ui.help - 1];
        const felucca_font_t *bf = text_w(&FONT_B, bn) <= 224 ? &FONT_B : text_w(&FONT_M, bn) <= 224 ? &FONT_M : &FONT_S;
        text_c(120, 18, bf, bn, K_BG);
    }
    for (k = 0; k < n; k++)
        text_c(120, 56 + k * 20, &FONT_S, l[k] + (l[k][0] == ' '), K_TEXT);
}

static void scale_text(char *b)
{
    const char *r = EN_NOTE_NAME[proj.par[P_ROOT]], *m = mode_name();
    int k = 0, i = 0;
    while (r[i] && k < 15) b[k++] = r[i++];
    if (k < 15) b[k++] = ' ';
    i = 0;
    while (m[i] && k < 15) b[k++] = m[i++];
    b[k] = 0;
}

static void num_text(int v, char *b)
{
    if (v < 0) { *b++ = '-'; v = -v; }
    itoa_u((uint32_t)v, b);
}

static void draw_mixer_channel(int32_t x, int32_t y, int32_t w, int32_t h, const char *name,
                               int value, int meter, uint16_t c)
{
    char t[8];
    int i, nseg = 8, seg_h = 2, gap = 1;
    int32_t mx = x + 8, bottom = y + h - 4;
    /* The expanded Mixer keeps the same eight-segment strip and value. */
    if (h > 100) { /* expanded GLO Mixer: thin horizontal segmented rungs */
        nseg=20;
        gap=5;
        seg_h=2;
        bottom=y+h-19;
        mx=x+11;
    }
    outline_box(x, y, w, h, c, K_PANEL);
    /* The compact meter beside an instrument identity card does not need
     * its instrument name repeated. Mixer strips keep their channel labels. */
    if (!(h == 38 && w == 57 &&
          (ui.view == V_PAD || ui.view == V_DRONE || ui.view == V_RINGS || ui.view == V_FM)))
        cv_text(x + 4, y + 2, &FONT_XS, name, c);
    for (i = 0; i < nseg; i++) {
        int32_t sy = bottom - (i + 1) * seg_h - i * gap;
        int threshold = ((i + 1) * 100 + nseg - 1) / nseg;
        cv_rect(mx, sy, h>100?w-22:12, seg_h, meter >= threshold ? c : K_LINE);
    }
    num_text(value, t);
    text_c(x + 38, y + h - 20, &FONT_S, t, K_TEXT);
}

static int fm_vu_value(void)
{
    /* FM publishes a real post-level audio peak. A linear 0..100 meter left
     * every segment dark below -18 dBFS, even for audible notes. Give its eight
     * segments useful audio thresholds without changing the engine's gain.
     * Thresholds: -60, -48, -36, -30, -24, -18, -12, -6 dBFS. */
    static const float threshold[8] = {0.001f, 0.00398107f, 0.01584893f, 0.03162278f,
                                      0.06309573f, 0.12589254f, 0.25118864f, 0.50118723f};
    float peak = en_fm_level;
    int n = 0;
    if (proj.par[P_FMLEVEL] <= 0 || dc_status() == 3) return 0;
    while (n < 8 && peak >= threshold[n]) ++n;
    return (n * 100 + 7) / 8;
}

static void fm_status_text(char text[40])
{
    const char *s;
    if (dc_status() == 3) {
        switch (dc_fault()) {
        case DC_FAULT_TIMEOUT: s = "TIMEOUT: RESTART"; break;
        case DC_FAULT_MAILBOX: s = "CHECK FAULT: RESTART"; break;
        case DC_FAULT_STACK: s = "STACK FAULT: RESTART"; break;
        default: s = "DSP FAULT: RESTART"; break;
        }
    } else if (proj.par[P_FMLEVEL] <= 0) s = "FM MUTED: RAISE LEVEL";
    else if (fm_port_active_voices > 0) {
        int n = fm_port_active_voices;
        *text++ = (char)('0' + n); *text++ = ' ';
        s = n == 1 ? "VOICE ACTIVE" : "VOICES ACTIVE";
    } else if (proj.par[P_MODE] == MODE_PIANO) s = "PIANO: PLAY KEYS OR MIDI CH 3";
    else if (!en_playing) s = "STOPPED: PRESS PLAY";
    else s = "WAITING FOR NOTES";
    while ((*text++ = *s++)) {}
}

static void mixer_vu_values(int vu[4])
{
    float bm = 0.0f;
    int i;
    for (i = 0; i < EN_NLOOP; i++) bm = fm_maxf(bm, en_loop_glow[i]);
    vu[0] = (int)(fm_clampf(en_pad_level * (float)proj.par[P_PAD] * 0.01f, 0.0f, 1.0f) * 100.0f + 0.5f);
    vu[1] = (int)(fm_clampf(bm * (float)proj.par[P_BELLS] * 0.01f, 0.0f, 1.0f) * 100.0f + 0.5f);
    vu[2] = (int)(fm_clampf(en_drone_level * (float)proj.par[P_PULSE] * 0.01f, 0.0f, 1.0f) * 100.0f + 0.5f);
    vu[3] = fm_vu_value();
}



/* Rows 1-3 are display-only. They never change the encoder mapping. */
static void sequencer_cell(int x, int y, int h, int slot, int view, int zone)
{
    int k=seq_binding(view,zone,slot);
    int fm=view==V_FMSEQ;
    int selected=ui.seq_zone[fm]==zone;
    uint16_t col=fm?K_FM:K_BELL;
    const char *name=seq_binding_label(view,zone,slot);
    char value[24]={0},num[3]={'K',(char)('1'+slot),0};
    uint16_t edge=selected?col:mix(K_LINE,K_BG,4);
    if(k!=K_NONE) knob_text(k,value);
    outline_box(x,y,slot==3?55:56,h,edge,selected?mix(K_PANEL,col,2):K_PANEL);
    if(k!=K_NONE)panel_text(x,y,slot==3?55:56,h,name,value,num,selected?col:K_DIM,selected?K_TEXT:K_DIM);

}

static void sequencer_main(void)
{
    int fm=ui.view==V_FMSEQ;
    int mode=proj.par[fm?P_FMSEQMODE:P_RSEQMODE];
    int active=fm?en_fm_seq_active_mode:en_ring_seq_active_mode;
    int zone=ui.seq_zone[fm];
    uint16_t col=fm?K_FM:K_BELL;
    const char *const labels[]={"ORIGINAL","HALLUCIGENIA","PROTEUS","UNIT-D"};
    const int xpos[]={28,99,168,218};
    outline_box(3,1,234,19,zone==0?col:mix(K_LINE,K_BG,5),K_PANEL);
    for(int i=0;i<4;++i)text_c(xpos[i],4,&FONT_T,labels[i],i==mode?col:K_DIM);
    outline_box(3,23,234,32,col,K_PANEL);
    if(mode==0) {
        int lengths[4]={13,17,23,29};
        for(int i=0;i<4;++i){
            int x=9+i*56;
            cv_rect(x,30,50,18,mix(col,K_BG,5));
            char t[8];itoa_u((uint32_t)lengths[i],t);
            text_c(x+25,32,&FONT_S,t,col);
        }
    } else if(mode==1) {
        int len=proj.par[fm?P_FMELENGTH:P_ELENGTH];
        int pos=fm?en_fm_evo_pos:en_evo_pos;
        for(int j=0;j<16;++j){
            int v=fm?en_fm_evo_trace[j]:en_evo_trace[j];
            int h=3+(v&127)*19/127;
            uint16_t tone=j>=len?K_PANEL:(v&128)?col:mix(col,K_BG,6);
            cv_rect(8+j*14,50-h,9,h,tone);
            if(j==pos)cv_rect(8+j*14,52,9,2,K_TEXT);
        }
    } else if(mode == 3) {
        /* Graph is an actual snapshot of the running UNIT-D DSP, not artwork. */
        en_unitd_screen_t graph;en_unitd_read(fm?1:0,&graph);
        int drawn=0;
        for(int j=0;j<graph.count&&drawn<46;j++)for(int k=j+1;k<graph.count&&drawn<46;k++){
            if(graph.edges[j]&(1ull<<k)){
                int x0=9+graph.x[j]*221/255,y0=28+graph.y[j]*22/255;
                int x1=9+graph.x[k]*221/255,y1=28+graph.y[k]*22/255;
                cv_line(x0,y0,x1,y1,mix(col,K_BG,8));drawn++;
            }
        }
        for(int j=0;j<graph.count;j++) {
            int x=9+graph.x[j]*221/255,y=28+graph.y[j]*22/255;
            cv_rect(x,y,2,2,mix(col,K_BG,3));
        }
        for(int j=0;j<graph.poly;j++)if(graph.active[j]<graph.count) {
            int q=graph.active[j],x=9+graph.x[q]*221/255,y=28+graph.y[q]*22/255;
            cv_rect(x-1,y-1,4,4,j?col:K_TEXT);
        }
    } else {
        int len=proj.par[fm?P_FPROTLEN:P_RPROTLEN];
        int pos=en_prot_pos[fm];
        for(int j=0;j<32;++j){
            int v=en_prot_trace[fm][j],h=3+(v&127)*19/127;
            uint16_t tone=j>=len?K_PANEL:(v&128)?col:mix(col,K_BG,6);
            cv_rect(8+j*7,50-h,5,h,tone);
            if(j==pos)cv_rect(8+j*7,52,5,2,K_TEXT);
        }
    }
    text_c(120,57,&FONT_T,active!=mode?"CHANGE ON NEXT BAR":
         zone==0?"SELECT: MODE  -  KNOB 1":
         zone==1?"SELECT: ROW 1  KNOBS 1-4":"SELECT: ROW 2  KNOBS 1-4",K_DIM);
    for(int i=0;i<4;++i)sequencer_cell(4+i*59,70,60,i,ui.view,1);
}

static void grid_cell(int view,int row,int slot,int y,int height);
static void unit_opt_cell(int slot,int row,int y,int ht){
    if(ui.view==V_RUNITOPT){grid_cell(ui.view,row,slot,y,ht);return;}
    int view=ui.view,fi=view==V_FUNITOPT,active=ui.unit_row[fi]==row;
    int x=4+slot*59,k=row==1?VIEW_KNOB[view][slot]:K_NONE;
    uint16_t col=fi?K_FM:K_BELL;
    outline_box(x,y,slot==3?55:56,ht,active?col:mix(K_LINE,K_BG,5),
                active?mix(K_PANEL,col,2):K_PANEL);
    if(k==K_NONE)return;
    char val[20];knob_text(k,val);
    panel_text(x,y,slot==3?55:56,ht,KNOB_LABEL[view][slot],val,slot==0?"K1":slot==1?"K2":slot==2?"K3":"K4",active?col:K_DIM,active?K_TEXT:K_DIM);
    if(active)cv_rect(x+4,y+ht-3,47,2,col);
}
static void unit_opt_main(void){
    int layer=ui.view==V_FUNITOPT;
    uint16_t col=layer?K_FM:K_BELL;
    char chord[16];chord_now(chord);
    outline_box(3,1,234,19,col,K_PANEL);
    cv_text(8,4,&FONT_T,"UNIT-D OPTIONS",col);
    cv_text(154,4,&FONT_T,"CHORD",K_DIM);
    cv_text(198,4,&FONT_T,chord,K_TEXT);
    outline_box(3,23,234,33,col,K_PANEL);
    en_unitd_screen_t graph;en_unitd_read(layer,&graph);
    int drawn=0;
    for(int j=0;j<graph.count&&drawn<46;j++)for(int k=j+1;k<graph.count&&drawn<46;k++){
        if(graph.edges[j]&(1ull<<k)){
            int x0=9+graph.x[j]*221/255,y0=28+graph.y[j]*22/255;
            int x1=9+graph.x[k]*221/255,y1=28+graph.y[k]*22/255;
            cv_line(x0,y0,x1,y1,mix(col,K_BG,8));drawn++;
        }
    }
    for(int j=0;j<graph.count;j++){
        int x=9+graph.x[j]*221/255,y=28+graph.y[j]*22/255;
        cv_rect(x,y,2,2,col);
    }
    text_c(120,57,&FONT_T,ui.view==V_RUNITOPT?"KNOBS 1-4":ui.unit_row[layer]==1?"SELECT: ROW 1  KNOBS 1-4":"SELECT: ROW 2  RESERVED",K_DIM);
    for(int i=0;i<4;i++)unit_opt_cell(i,1,70,59);
}
static void fm_filter_cell(int slot,int row,int y,int ht) {
    int k=fm_filter_knob(ui.view,row,slot), x=4+59*slot;
    int active=ui.fm_filter_row==row;
    uint16_t edge=active?K_FM:mix(K_LINE,K_BG,4);
    outline_box(x,y,slot==3?55:56,ht,edge,active?mix(K_PANEL,K_FM,2):K_PANEL);
    if(k==K_NONE)return;
    const char *label=row==2?"Model":slot==2?"Reson.":KNOB_LABEL[V_FMFILTER][slot];
    char value[20]; knob_text(k,value);
    if(k==P_FFILTERMODEL && proj.par[k]==FM_FILTER_CLEAN) {
        const char *src="Clean SVF";char *dst=value;while((*dst++=*src++)){};
    }
    panel_text(x,y,slot==3?55:56,ht,label,value,slot==0?"K1":slot==1?"K2":slot==2?"K3":"K4",active?K_FM:K_DIM,active?K_TEXT:K_DIM);
    if(active)cv_rect(x+4,y+ht-3,47,2,K_FM);
}
static void mod_shape_cell(int slot,int row,int y,int ht) {
    if((row==1 && slot==2)||(row==2 && slot==1))return;
    int k=mod_view_knob(row,slot),x=4+59*slot,active=ui.mod_row==row;
    int w=((row==1&&slot==1)||(row==2&&slot==0))?115:slot==3?55:56;
    uint16_t col=page_colour(V_MOD);
    outline_box(x,y,w,ht,active?col:mix(K_LINE,K_BG,4),active?mix(K_PANEL,col,2):K_PANEL);
    if(k==K_NONE)return;
    char val[20];knob_text(k,val);
    if(ui.mod_route<6 && k==K_MOD_SHAPE){const char *src="Smooth Rnd";char *dst=val;while((*dst++=*src++)){} }
    if(k==K_MOD_SHAPE && ui.mod_route>=6) {
        static const char *const names[]={"Smooth Rnd","Step Rnd","Sine","Pulse","Triangle","Saw"};
        int shape=knob_get(k);if(shape>=0&&shape<6) { const char *src=names[shape];char *dst=val;while((*dst++=*src++)){} }
    }
    const char *label=k==K_MOD_SHAPE?"Shape":k==K_MOD_SPEED?"Speed":k==K_MOD_DEPTH?"Depth":k==K_MOD_DEST?"Destination":"Mod";
    panel_text(x,y,w,ht,label,val,(ui.mod_route<6 && (k==K_MOD_SHAPE || k==K_MOD_DEST))?"Fixed":slot==0?"K1":slot==1?"K2":slot==2?"K3":"K4",active?col:K_DIM,active?K_TEXT:K_DIM);

}
/* Visual approximation of the active analog model's magnitude response.
 * This display is never evaluated in the audio interrupt. */
static int filter_response_y(int x) {
    int model=proj.par[P_FFILTERMODEL],type=proj.par[P_FFILTERTYPE];
    int cut=30+(proj.par[P_FFILTERCUT]*177)/100;
    int d=x-cut;
    if(model==FM_FILTER_BYPASS)return 28;
    if(model==FM_FILTER_JUNO || model==FM_FILTER_POLYSIX)type=0;
    if(model==FM_FILTER_CS80) {
        int low=cut-38;
        if(x<low)return x<low-35?47:24+(low-x)*23/35;
        return d<0?24:d>75?47:24+d*23/75;
    }
    if(type==1) return d<-20?46:d>20?22:46-(d+20)*24/40;
    if(type==2) {int a=d<0?-d:d;return a<24?23: a>70?46:23+(a-24)*23/46;}
    if(type==3){int a=d<0?-d:d;return a<15?46:a>60?24:46-(a-15)*22/45;}
    return d<0?24:d>75?47:24+d*23/75;
}

/* Compact 2x4 cells shared by HOME, Pad, Rings, FM and Reverb views.
 * Main grid begins at 70px below page body to keep a small musical-info area.
 * Setup is deliberately just the settings grid, without an info panel.
 * Empty controls keep their visible border and no label/encoder action. */
static int grid_label_is(const char *a,const char *b)
{
    while(*a && *b && *a==*b) {a++;b++;}
    return *a==0 && *b==0;
}
static void grid_cell(int view,int row,int slot,int y,int height)
{
    if(wide_grid_page(view)) {
        if(slot!=0 && slot!=2)return;
        int index=(row-1)*2+slot/2,x=4+slot*59,w=114;
        uint16_t col=page_colour(view);char value[24];
        int k=VIEW_KNOB[view][index];
        outline_box(x,y,w,height,col,K_PANEL);
        if(k==K_NONE)return;
        knob_text(k,value);
        static const char *const knobs[]={"K1","K2","K3","K4"};
        panel_text(x,y,w,height,KNOB_LABEL[view][index],value,knobs[index],col,K_DIM);

        return;
    }
    int x=4+slot*59, k=grid_binding(view,row,slot);
    int active=grid_row(view)==row;
    int hot=active && plat_ms()<ui.touch_until && ui.touched==slot;
    uint16_t color=page_colour(view), edge=active?color:mix(K_LINE,K_BG,4);
    char value[24];
    outline_box(x,y,slot==3?55:56,height,edge,active?mix(K_PANEL,color,2):K_PANEL);
    if(k==K_NONE)return; /* reserved empty box */
    knob_text(k,value);
    /* On the FM-1, a 56px cell cannot show every long source label at once.
     * Abbreviate only the displayed caption; keep the real parameter and
     * screen-reader names unchanged. */
    const char *label=grid_label(view,row,slot);
    if(grid_label_is(label,"Structure"))label="Struct";
    else if(grid_label_is(label,"Brightness"))label="Bright";
    else if(grid_label_is(label,"Damping"))label="Damp";
    else if(grid_label_is(label,"Position"))label="Pos";
    else if(grid_label_is(label,"Feedback"))label="Fdbk";
    else if(grid_label_is(label,"Polyphony"))label="Poly";
    else if(grid_label_is(label,"Diffusion"))label="Diffus";
    else if(grid_label_is(label,"Oct Drift"))label="OctDrft";
    else if(grid_label_is(label,"Resonance"))label="Reso";
    panel_text(x,y,slot==3?55:56,height,label,value,slot==0?"K1":slot==1?"K2":slot==2?"K3":"K4",active?color:K_DIM,hot?color:active?K_TEXT:K_DIM);

}
static void grid_context(int view)
{
    char chord[16],value[24];
    const char *left="SETTINGS",*right="CHORD",*right_value=chord;
    uint16_t color=page_colour(view);
    chord_now(chord);
    if(view==V_GARDEN || view==V_ROOTDIV) {
        left="CURRENT CHORD";
        caps(value,piece_name(proj.piece),sizeof value);
        right="PIECE";right_value=value;
        outline_box(2,1,236,64,color,K_PANEL);
        cv_text(7,3,(text_w(&FONT_XS,left)<=110?&FONT_XS:&FONT_T),left,K_DIM);
        text_c(61,22,&FONT_M,chord,color);
        cv_text(125,3,(text_w(&FONT_XS,right)<=108?&FONT_XS:&FONT_T),right,K_DIM);
        text_c(179,22,panel_fit(value,104,25),value,K_TEXT);
        text_c(120,47,&FONT_T,view==V_ROOTDIV?"GARDEN PAGE 2 - KNOBS 1-3":wide_grid_page(view)?"GARDEN PAGE 1 - KNOBS 1-4":grid_row(view)==1?"HOME + SELECT: ROW 1":"HOME + SELECT: ROW 2  ROOT",K_DIM);
        return;
    }
    if(view==V_PAD||view==V_DRONE||view==V_RINGS||view==V_FM) {
        int k=view==V_PAD?P_PADTONE:view==V_DRONE?P_DRONEVOICE:view==V_RINGS?P_RMODEL:P_FMPATCH;
        left=view==V_PAD?"PAD 1 COLOUR":view==V_DRONE?"PAD 2 COLOUR":view==V_RINGS?"RINGS MODEL":"FM PATCH";
        if(view==V_FM) {
            const char *name=fm_port_patch_name(proj.par[P_FMPATCH]);
            caps(value,name,sizeof value);
        } else en_param_text(k,proj.par[k],value);
    } else if(view==V_PADBODY||view==V_DRONEVOICE) {
        left=view==V_PADBODY?"PAD 1 BODY":"PAD 2 BODY";
        num_text(proj.par[view==V_PADBODY?P_BTONE:P_HATS],value);
    } else if(view==V_PADSEQ||view==V_PAD2SEQ) {
        left=view==V_PADSEQ?"PAD 1 SEQUENCE":"PAD 2 SEQUENCE";
        en_param_text(view==V_PADSEQ?P_PADOCT:P_PAD2OCT,proj.par[view==V_PADSEQ?P_PADOCT:P_PAD2OCT],value);
    } else if(view==V_RINGTONE||view==V_RINGEXC||view==V_RINGDLY||view==V_DAMBIENT||view==V_RPROTOPT) {
        left=view==V_RINGTONE?"RINGS TONE":view==V_RINGEXC?"RINGS EXCITER":view==V_RINGDLY?"RINGS DELAY":view==V_DAMBIENT?"RINGS DELAY 2":"PROTEUS OPTIONS";
        en_param_text(P_RMODEL,proj.par[P_RMODEL],value);
    } else if(view==V_SPACE||view==V_REVSEND||view==V_REVMIX) {
        left=view==V_SPACE?"MONUMATIC REVERB":view==V_REVSEND?"REVERB SENDS":"MONUMATIC REVERB";
        en_param_text(P_REVSPACE,proj.par[P_REVSPACE],value);
        right="TYPE";right_value="FULL RATE";
    } else {
        left="SETTINGS";value[0]=0;
    }
    /* Retain only a small status area above the setting grid; no VU meters. */
    outline_box(2,1,236,64,color,K_PANEL);
    cv_text(7,3,(text_w(&FONT_XS,left)<=110?&FONT_XS:&FONT_T),left,color);
    if(view==V_SPACE||view==V_REVSEND||view==V_REVMIX) {
        en_param_text(P_REVSPACE,proj.par[P_REVSPACE],value);
        text_c(67,21,panel_fit(value,107,25),value,K_TEXT);
    } else {
        const felucca_font_t *f=panel_fit(value,107,25);
        text_c(66,21,f,value,K_TEXT);
    }
    cv_text(125,3,(text_w(&FONT_XS,right)<=108?&FONT_XS:&FONT_T),right,K_DIM);
    text_c(181,21,panel_fit(right_value,98,25),right_value,K_TEXT);
    text_c(120,47,&FONT_T,wide_grid_page(view)?"KNOBS 1-4":grid_row(view)==1?"SELECT: ROW 1  KNOBS 1-4":"SELECT: ROW 2  KNOBS 1-4",K_DIM);
}
static void draw_main(void)
{
    char chord[16], t0[16], t1[16], t2[20], t3[16], t4[16];
    uint32_t h = 2166136261u;
    int i;
    /* Mixer panel 4 owns the whole body. Invalidate the previous page's
     * main band so a stopped/muted round trip still restores its cards. */
    if (ui.view == V_MIXER || ui.view == V_SETUP) { ui.sig[1] = 0; return; }
    if (ui.frame % 2u) return;
    if (ui.view == V_PERF) {
        perf_stats_t s; char v[16]; perf_read(&s);
        const char *labels[8] = {"Audio avg %", "Audio peak %", "CPU0 audio us", "CPU0 peak us", "CPU1 work us", "Wait us", "Dropouts", "Faults"};
        uint32_t vals[8] = {s.audio_pct,s.peak_pct,s.cpu0_us,s.cpu0_peak_us,s.worker_us,s.wait_us,s.dropouts,s.faults};
        cv_begin(240,MAIN_H,K_BG);
        for (int j=0;j<8;++j) {
            itoa_u(vals[j],v);
            value_box(2+(j%2)*119,1+(j/2)*33,117,31,labels[j],v,K_ACC,K_PANEL,0);
        }
        cv_blit(0,MAIN_Y); return;
    }

    h = hash(h, (uint32_t)ui.view);
    if(ui.view==V_RUNITOPT || ui.view==V_FUNITOPT){
        int fi=ui.view==V_FUNITOPT;
        h=hash(h,ui.unit_row[fi]);
        for(int i=0;i<4;i++)h=hash(h,(uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
        h=hash(h,ui.frame/5);
    }
    if (ui.view==V_RINGSEQ || ui.view==V_FMSEQ) {
        int fm=ui.view==V_FMSEQ;
        h=hash(h,ui.seq_zone[fm]);
        h=hash(h,proj.par[fm?P_FMSEQMODE:P_RSEQMODE]);
        for(int row=1;row<=2;++row)
            for(int slot=0;slot<4;++slot)
                h=hash(h,(uint32_t)knob_get(seq_binding(ui.view,row,slot)));
        h=hash(h,en_prot_pos[fm]);
        if(proj.par[fm?P_FMSEQMODE:P_RSEQMODE]==3)h=hash(h,ui.frame/5);
        h=hash(h,((uint32_t)en_ring_seq_active_mode<<8)|en_fm_seq_active_mode);
    }
    if(ui.view==V_RINGSEQ || ui.view==V_ESETUP || ui.view==V_EPATTERN || fm_view(ui.view)) h=hash(h,ui.frame/4);
    if (fm_view(ui.view)) {
        for (i = P_FMLEVEL; i <= P_FMEPATTERN; ++i) h = hash(h, (uint32_t)proj.par[i]);
        h = hash(h, (uint32_t)fm_vu_value());
        h = hash(h, (uint32_t)fm_port_active_voices | (uint32_t)dc_status() << 8 | (uint32_t)dc_fault() << 16);
    }
    /* Native layout proposal: overview cards must refresh even while muted
     * or stopped. The encoder mappings and page navigation stay unchanged. */
    if (ui.view == V_PAD || ui.view == V_DRONE || ui.view == V_RINGS) {
        static const uint8_t overview_params[] = {
            P_PADTONE, P_DRONEVOICE, P_RMODEL,
            P_PADREV, P_DRONEREV, P_RINGREV,
            P_PADOCT, P_PAD2OCT, P_BOCT,
            P_PADCHAR, P_PAD2CHAR, P_RDELAYMIX, P_RSEQMODE
        };
        for (i = 0; i < (int)(sizeof overview_params / sizeof overview_params[0]); ++i)
            h = hash(h, (uint32_t)proj.par[overview_params[i]]);
        h = hash(h, (uint32_t)en_playing | (uint32_t)en_frozen << 1 |
                    (uint32_t)en_evo_active << 2 | (uint32_t)dc_status() << 8 |
                    (uint32_t)dc_fault() << 16);
    }
    if (rings_aux_grid_page()) {
        /* Values of controls in the upper grid must invalidate MAIN too. */
        for (i=0; i<4; ++i)
            h=hash(h,(uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
        h=hash(h,(uint32_t)ui.touched | (uint32_t)(plat_ms()<ui.touch_until)<<8);
    }
    if (ui.view == V_FMFILTER) {
        h=hash(h,ui.fm_filter_row);
        for(int p=P_FFILTERMODEL;p<=P_FFILTERMIX;p++)h=hash(h,proj.par[p]);
    }
    if (grid_page(ui.view)) {
        h=hash(h,grid_row(ui.view));
        for(int row=1;row<=2;row++)for(int k=0;k<4;k++)
            h=hash(h,(uint32_t)knob_get(grid_binding(ui.view,row,k)));
        h=hash(h,(uint32_t)ui.touched | (uint32_t)(plat_ms()<ui.touch_until)<<8);
    }
    if (ui.view == V_MOD) {
        h = hash(h, ui.mod_row);
        if(ui.mod_route<11) {
            for(int i=0;i<4;i++)h=hash(h,(uint32_t)knob_get(mod_view_knob(1,i)));
            h=hash(h,(uint32_t)knob_get(K_MOD_SHAPE));
        }
        h = hash(h, ui.frame / 4);
        h = hash(h, ui.mod_route);
    }
    h = hash(h, proj.piece | (uint32_t)proj.par[P_TEMPO] << 8 | (uint32_t)proj.par[P_EVOLVE] << 16 | (uint32_t)proj.par[P_MOTION] << 24);
    h = hash(h, (uint32_t)proj.par[P_DRIFT] | (uint32_t)proj.par[P_MODE] << 8 | (uint32_t)proj.par[P_ROOT] << 16 | (uint32_t)proj.pool << 20);
    h = hash(h, (uint32_t)proj.par[P_PAD] | (uint32_t)proj.par[P_BELLS] << 8 | (uint32_t)proj.par[P_PULSE] << 16 | (uint32_t)proj.par[P_FMLEVEL] << 24);
    {
        int vu[4]; mixer_vu_values(vu);
        h = hash(h, (uint32_t)(vu[0] | vu[1] << 8 | vu[2] << 16 | vu[3] << 24));
    }
    for (i = 0; i < EN_NPAD; i++) h = hash(h, en_chord[i]);
    if (h == ui.sig[1]) return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);

    if(grid_page(ui.view)) {
        grid_context(ui.view);
        for(int i=0;i<4;i++)grid_cell(ui.view,1,i,65,66);
        cv_blit(0,MAIN_Y);return;
    }
    if(ui.view==V_RUNITOPT || ui.view==V_FUNITOPT){
        unit_opt_main();cv_blit(0,MAIN_Y);return;
    }
    if(ui.view==V_FMFILTER || ui.view==V_PAD2FILTER) {
        uint16_t col=K_FM;
        outline_box(2,1,236,54,col,K_PANEL);
        cv_text(8,3,&FONT_T,"FILTER RESPONSE",col);
        cv_text(176,3,&FONT_T,proj.par[P_FFILTERMODEL]==FM_FILTER_BYPASS?"BYPASS":proj.par[P_FFILTERMODEL]==FM_FILTER_CS80?"HP + LP":proj.par[P_FFILTERMODEL]>=FM_FILTER_JUNO?"24 dB/OCT":"12 dB/OCT",K_DIM);
        for(int x=20;x<=220;x+=40)cv_line(x,19,x,49,K_LINE);
        cv_line(20,34,220,34,K_PANEL);
        int py=filter_response_y(20);
        for(int x=21;x<=220;x+=2){int y=filter_response_y(x);cv_line(x-1,py,x,y,col);py=y;}
        cv_text(19,43,&FONT_T,"50Hz",K_DIM);cv_text(97,43,&FONT_T,"1k",K_DIM);
        cv_text(145,43,&FONT_T,"4k",col);cv_text(196,43,&FONT_T,"16k",K_DIM);
        text_c(120,57,&FONT_T,ui.fm_filter_row==1?"SELECT: ROW 1  KNOBS 1-4":"SELECT: ROW 2  K1 MODEL",K_DIM);
        for(int i=0;i<4;i++)fm_filter_cell(i,1,70,59);
        cv_blit(0,MAIN_Y);return;
    }
    if(ui.view==V_MOD && ui.mod_route<11) {
        int row=ui.mod_route,rp=mod_param(K_MOD_SHAPE),shape=rp>=0?proj.par[rp]:0;
        uint16_t col=page_colour(V_MOD);
        outline_box(2,1,236,64,col,K_PANEL);
        cv_text(8,4,&FONT_T,row==0?"M1 MOD SOURCE":row==1?"M2 MOD SOURCE":row==2?"M3 MOD SOURCE":row==3?"M4 MOD SOURCE":row==4?"M5 MOD SOURCE":row==5?"M6 MOD SOURCE":row==9?"M10 FM CUTOFF":row==10?"M11 FM RESONANCE":row==6?"M7 MOD SOURCE":row==7?"M8 MOD SOURCE":"M9 MOD SOURCE",col);
        for(int x=10;x<=230;x+=44)cv_line(x,19,x,45,K_LINE);
        cv_line(10,32,230,32,K_LINE);
        /* Waveform preview; random shapes show a deterministic example contour. */
        int lastx=11,lasty=32;
        static const int rnd[9]={-8,17,-12,9,-18,16,4,-6,12};
        for(int x=13;x<=229;x+=2) {
            int q=x-11,seg=q/27;
            float ph=(float)q/218.0f;int y=32;
            if(shape==0){int i=seg>7?7:seg;float t=(float)(q-i*27)/27.0f;
                t=t*t*(3.0f-2.0f*t);y=32-(int)(rnd[i]*(1.0f-t)+rnd[i+1]*t);}
            else if(shape==1) y=32-rnd[seg>8?8:seg];
            else if(shape==2) y=32-(int)(19.0f*fm_sinf(ph*FM_TWO_PI));
            else if(shape==3) y=32-(ph<0.25f?19:-19);
            else if(shape==4) y=32-(int)(19.0f*(ph<0.5f?4.0f*ph-1.0f:3.0f-4.0f*ph));
            else y=32-(int)(19.0f*(2.0f*ph-1.0f));
            cv_line(lastx,lasty,x,y,col);lastx=x;lasty=y;
        }
        text_c(120,49,&FONT_T,ui.mod_row==1?"SELECT: ROW 1  KNOBS 1-4":(ui.mod_route<6?"SELECT: ROW 2  K3 SPEED":"SELECT: ROW 2  SHAPE / SPEED"),K_DIM);
        for(int i=0;i<4;i++)mod_shape_cell(i,1,65,66);
        cv_blit(0,MAIN_Y);return;
    }
    /* The Mod page puts its controls immediately below the header. */
    if (ui.view == V_MOD) {
        int r = ui.mod_route < 9 ? ui.mod_route : 0;
        int count = en_mod_trace_count, head = en_mod_trace_head;
        int start = (head + EN_MOD_TRACE - count) % EN_MOD_TRACE;
        int depth = knob_get(K_MOD_DEPTH);
        uint16_t colour = page_colour(V_MOD);
        outline_box(1, 1, 238, 130, colour, K_BG);
        for (int gx = 8; gx <= 232; gx += 56) cv_line(gx, 9, gx, 119, K_PANEL);
        cv_line(8, 64, 232, 64, K_LINE);
        for (int j = 1; j < count; j++) {
            int x = 9 + EN_MOD_TRACE - count + j;
            int a = en_mod_trace[r][(start + j - 1) % EN_MOD_TRACE];
            int b = en_mod_trace[r][(start + j) % EN_MOD_TRACE];
            cv_line(x - 1, 64 - a * depth * 52 / 12700,
                    x, 64 - b * depth * 52 / 12700, colour);
        }
        cv_blit(0, MAIN_Y + KNB_H);
        return;
    }

    if (ui.view==V_RINGSEQ || ui.view==V_FMSEQ) {
        sequencer_main();cv_blit(0,MAIN_Y);return;
    }

    /* Main instrument overview: the exact FM card geometry, with each
     * instrument's existing identity, meter and applicable detail values. */
    if (ui.view == V_PAD || ui.view == V_DRONE || ui.view == V_RINGS) {
        int rings = ui.view == V_RINGS, pad2 = ui.view == V_DRONE;
        int level = rings ? P_BELLS : pad2 ? P_PULSE : P_PAD;
        int identity = rings ? P_RMODEL : pad2 ? P_DRONEVOICE : P_PADTONE;
        int send = rings ? P_RINGREV : pad2 ? P_DRONEREV : P_PADREV;
        int octave = rings ? P_BOCT : pad2 ? P_PAD2OCT : P_PADOCT;
        int detail = rings ? P_RDELAYMIX : pad2 ? P_PAD2CHAR : P_PADCHAR;
        int vu[4];
        uint16_t colour = page_colour(ui.view);
        const char *status;
        char fault[40];
        mixer_vu_values(vu);
        en_param_text(identity, proj.par[identity], t0);
        chord_now(chord);
        value_box(2,1,176,38,rings ? "RINGS MODEL" : pad2 ? "PAD 2 COLOUR" : "PAD 1 COLOUR",
                  t0,colour,K_PANEL,0);
        draw_mixer_channel(181,1,57,38,rings ? "RINGS" : pad2 ? "PAD 2" : "PAD 1",
                           proj.par[level],vu[rings ? 1 : pad2 ? 2 : 0],colour);
        if (rings) {
            value_box(2,42,116,38,"SEQUENCER",
                  en_ring_seq_active_mode==2?"PROTEUS":en_ring_seq_active_mode==1?"HALLUCIGENIA":"ORIGINAL",colour,K_PANEL,0);
        } else {
            num_text(EN_NPAD,t1);
            value_box(2,42,116,38,"PAD VOICES",t1,colour,K_PANEL,0);
        }
        value_box(121,42,117,38,pad2 ? "GLOBAL CHORD" : "CURRENT CHORD",chord,K_LINE,K_PANEL,0);
        num_text(proj.par[send],t0);
        value_box(2,83,77,35,"REV SEND",t0,K_LINE,K_PANEL,0);
        en_param_text(octave,proj.par[octave],t1);
        value_box(82,83,77,35,"OCTAVE",t1,K_LINE,K_PANEL,0);
        num_text(proj.par[detail],t2);
        value_box(162,83,76,35,rings ? "DELAY MIX" : "CHARACTER",t2,K_LINE,K_PANEL,0);
        if (rings && dc_status() == 3) {
            fm_status_text(fault); status = fault;
        } else if (proj.par[level] <= 0) {
            status = rings ? "RINGS MUTED: RAISE MIXER" :
                     pad2 ? "PAD 2 MUTED: RAISE MIXER" : "PAD 1 MUTED: RAISE MIXER";
        } else if (rings && proj.par[P_MODE] == MODE_PIANO) status = "PIANO: PLAY KEYS";
        else if (!en_playing) status = "STOPPED: PRESS PLAY";
        else if (en_frozen) status = "FROZEN: CHORD HELD";
        else status = "GENERATIVE PLAYBACK";
        text_c(120,121,&FONT_XS,status,K_DIM);
        cv_blit(0,MAIN_Y); return;
    }
    if (ui.view == V_FM) {
        const char *patch = fm_port_patch_name(proj.par[P_FMPATCH]);
        char status[40];
        int meter = fm_vu_value();
        chord_now(chord);
        value_box(2,1,176,38,"SIX-OP FM PATCH",patch,K_FM,K_PANEL,0);
        draw_mixer_channel(181,1,57,38,"FM",proj.par[P_FMLEVEL],meter,K_FM);
        value_box(2,42,116,38,"SEQUENCER",
                  en_fm_seq_active_mode==2?"PROTEUS":en_fm_seq_active_mode==1?"HALLUCIGENIA":"ORIGINAL",K_FM,K_PANEL,0);
        value_box(121,42,117,38,"CURRENT CHORD",chord,K_LINE,K_PANEL,0);
        num_text(proj.par[P_FMREV],t0);
        value_box(2,83,77,35,"REV SEND",t0,K_LINE,K_PANEL,0);
        en_param_text(P_FMOCT,proj.par[P_FMOCT],t1);
        value_box(82,83,77,35,"OCTAVE",t1,K_LINE,K_PANEL,0);
        value_box(162,83,76,35,"MIDI IN","CH 3",K_LINE,K_PANEL,0);
        fm_status_text(status);
        text_c(120,121,&FONT_XS,status,K_DIM);
        cv_blit(0,MAIN_Y); return;
    }
    if (ui.view==V_RPROTOPT || ui.view==V_FPROTOPT) {
        uint16_t c=page_colour(ui.view);
        int fi=ui.view==V_FPROTOPT;
        int b=fi?P_FPROTLEN:P_RPROTLEN;
        chord_now(chord);
        value_box(2,2,236,32,"PROTEUS OPTIONS",fi?"FM MELODY":"RINGS MELODY",c,K_PANEL,0);
        value_box(2,38,116,40,"ACTIVE ORIGINAL CHORD",chord,c,K_PANEL,0);
        num_text(proj.par[b],t0);
        value_box(122,38,116,40,"LENGTH",t0,c,K_PANEL,0);
        text_c(120,99,&FONT_XS,"ROTATE KNOB 2 TO CREATE A NEW MELODY",K_DIM);
        cv_blit(0,MAIN_Y);return;
    }
    if (ui.view==V_RINGSEQ || ui.view==V_ESETUP || ui.view==V_EPATTERN ||
        ui.view==V_FMMOTION || ui.view==V_FMSEQ || ui.view==V_FMESETUP || ui.view==V_FMEPATTERN) {
        int isfm=fm_view(ui.view);
        int active=isfm?en_fm_evo_active:en_evo_active;
        int pattern=isfm?en_fm_evo_pattern:en_evo_pattern;
        int pos=isfm?en_fm_evo_pos:en_evo_pos;
        int pmode=isfm?P_FMSEQMODE:P_RSEQMODE, ppattern=isfm?P_FMEPATTERN:P_EPATTERN;
        int plength=isfm?P_FMELENGTH:P_ELENGTH, pfollow=isfm?P_FMEFOLLOW:P_EFOLLOW;
        int evolving=isfm?(ui.view==V_FMESETUP || ui.view==V_FMEPATTERN || active):(ui.view!=V_RINGSEQ || active);
        int pending=proj.par[pmode]!=active || proj.par[ppattern]!=pattern+1;
        uint16_t colour=isfm?K_FM:K_BELL, dim=isfm?K_FM_T:K_BELL_T;
        chord_now(chord);
        value_box(2,1,116,30,isfm?"FM SEQUENCER":"SEQUENCER",active?"EVOLVE":"ORIGINAL",colour,K_PANEL,0);
        value_box(121,1,117,30,proj.par[pfollow]?"CHORD":"SCALE",proj.par[pfollow]?chord:"ROOT + SCALE",K_LINE,K_PANEL,0);
        if(evolving) {
            for(int j=0;j<16;j++) {
                int x=5+j*14, v=isfm?en_fm_evo_trace[j]:en_evo_trace[j], h=5+(v&127)*39/127;
                if(j<proj.par[plength]) {
                    cv_rect(x,80-h,10,h,(v&128)?colour:dim);
                    cv_rect(x,85,10,4,en_playing&&active&&j==pos?K_TEXT:K_LINE);
                }
            }
            char line[32], *b=line;
            const char *a="PATTERN ";while(*a)*b++=*a++;
            itoa_u(pattern+1,b);while(*b)b++;
            a="  STEP ";while(*a)*b++=*a++;
            itoa_u(pos+1,b);while(*b)b++;
            *b++='/';itoa_u(proj.par[plength],b);
            text_c(120,96,&FONT_XS,line,K_TEXT);
            text_c(120,114,&FONT_XS,pending?"CHANGE ON NEXT BAR":
                   ui.view==V_EPATTERN || ui.view==V_FMEPATTERN?"SAVE: KEEP ALL PATTERNS":
                   ui.view==V_ESETUP || ui.view==V_FMESETUP?"MUTATION AT LOOP END":
                   ui.view==V_FMMOTION?"DENSITY: ORIGINAL / BURST: BOTH":
                   isfm?"SEL: FM EVOLVE SETUP":"RINGS: EVOLVE SETUP",K_DIM);
        } else {
            value_box(2,36,116,38,"LOOP 1","13 STEPS",colour,K_PANEL,0);
            value_box(121,36,117,38,"LOOP 2","17 STEPS",colour,K_PANEL,0);
            value_box(2,77,116,38,"LOOP 3","23 STEPS",colour,K_PANEL,0);
            value_box(121,77,117,38,"LOOP 4","29 STEPS",colour,K_PANEL,0);
            text_c(120,119,&FONT_XS,pending?"CHANGE ON NEXT BAR":isfm?"FM LOOPS + BURST":"ORIGINAL LOOPS + YOUR BURST",K_DIM);
        }
        cv_blit(0,MAIN_Y); return;
    }
    chord_now(chord);
    num_text(proj.par[P_TEMPO], t0);
    num_text(proj.par[P_EVOLVE], t1);
    num_text(proj.par[P_MOTION], t2);
    en_param_text(P_DRIFT, proj.par[P_DRIFT], t3);

    /* Row 1: Garden/Home status only. */
    value_box(2, 1, 72, 36, "CHORD", chord, K_LINE, K_PANEL, 0);
    value_box(76, 1, 39, 36, "TEMPO", t0, K_LINE, K_PANEL, 0);
    value_box(117, 1, 39, 36, "EVOLVE", t1, K_LINE, K_PANEL, 0);
    value_box(158, 1, 39, 36, "MOTION", t2, K_LINE, K_PANEL, 0);
    value_box(199, 1, 39, 36, "DRIFT", t3, K_LINE, K_PANEL, 0);

    /* Row 2: one active preset, mode, and the complete scale name. */
    caps(t0, piece_name(proj.piece), sizeof t0);
    caps(t1, mode_name(), sizeof t1);
    scale_text(t2); caps(t4, t2, sizeof t4);
    value_box(2, 39, 92, 24, "PRESET", t0, K_LINE, K_PANEL, 0);
    value_box(96, 39, 67, 24, "MODE", t1, K_LINE, K_PANEL, 0);
    value_box(165, 39, 73, 24, "SCALE", t4, K_LINE, K_PANEL, 0);

    /* Proposal: hide the redundant live mixer strip on the Rings tone,
     * exciter, delay and ambient-delay pages. Keep their headings, status
     * cards and encoder controls unchanged. The GLO Mixer is unaffected. */
    if (rings_aux_grid_page()) {
        draw_rings_aux_grid();
    } else {
        int vu[4]; mixer_vu_values(vu);
        outline_box(1, 65, 238, 67, K_ACC, K_BG);
        draw_mixer_channel(4, 68, 56, 63, "PAD", proj.par[P_PAD], vu[0], K_PAD);
        draw_mixer_channel(63, 68, 56, 63, "RINGS", proj.par[P_BELLS], vu[1], K_BELL);
        draw_mixer_channel(122, 68, 56, 63, "PAD 2", proj.par[P_PULSE], vu[2], K_PULSE);
        draw_mixer_channel(181, 68, 55, 63, "FM", proj.par[P_FMLEVEL], vu[3], K_FM);
    }
    cv_blit(0, MAIN_Y);
}



/* the knob strip (X0X's): a rule with the page's name and five page dots at its right end; under
 * it a cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The
 * knob being turned is drawn on a tint. Only redrawn when what it shows changes (the rings cost a
 * square root and an arctangent per pixel). */
static void draw_help_row4(uint16_t c)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    outline_box(3, 2, 234, 68, c, K_PANEL);
    text_c(120, 5, &FONT_S, BTN_LABEL[ui.help - 1], c);
    for (k = 0; k < n; k++) text_c(120, 23 + k * 11, &FONT_XS, l[k] + (l[k][0] == ' '), K_TEXT);
}

/* Row 4 is the only encoder-assigned area. HOME duplicates Row 1 here; CHORD remains status-only,
 * while the four physical knobs edit Tempo/Evolve/Motion/Drift exactly as before. */
/* Approved 2x2 control grid on instrument overviews and MOD LFO.
 * Reuse the existing 72-pixel encoder band and the same font/colour palette.
 * A compact value font prevents long presets or destinations from running into
 * their neighbouring box. The four physical knob slots are unchanged. */
static void value_box_compact(int32_t x, int32_t y, int32_t w, int32_t h,
                              const char *label, const char *value, uint16_t edge,
                              uint16_t fill, int hot)
{
    const felucca_font_t *f = &FONT_S;
    uint16_t bg = hot ? mix(fill, edge, 4) : fill;
    outline_box(x, y, w, h, edge, bg);
    cv_text(x + 4, y + 1, &FONT_T, label, hot ? edge : K_DIM);
    if (text_w(f, value) > w - 8) f = &FONT_XS;
    if (text_w(f, value) > w - 8) f = &FONT_T;
    text_c(x + w / 2, y + (f == &FONT_T ? 17 : 14), f, value, hot ? edge : K_TEXT);
}

static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)(ui.touched + 1) << 8 | (uint32_t)ui.help << 16), i;
    int now_touch = plat_ms() < ui.touch_until;
    uint16_t c = page_colour(ui.view);
    int32_t knob_y = KNB_Y;
    for (i = 0; i < 4; i++) h = hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
    if(ui.view==V_MOD && ui.mod_route<11) {h=hash(h,ui.mod_row);h=hash(h,(uint32_t)knob_get(K_MOD_SHAPE));}
    if(ui.view==V_FMFILTER){h=hash(h,ui.fm_filter_row);h=hash(h,proj.par[P_FFILTERMODEL]);}
    if(ui.view==V_RUNITOPT || ui.view==V_FUNITOPT)h=hash(h,ui.unit_row[ui.view==V_FUNITOPT]);
    if (ui.view==V_RINGSEQ || ui.view==V_FMSEQ) {
        int row=ui.seq_zone[ui.view==V_FMSEQ];
        h=hash(h,(uint32_t)row);
        for (int i=0;i<4;++i) h=hash(h,(uint32_t)knob_get(seq_binding(ui.view,row?row:1,i)));
    }
    if (ui.view == V_PERF) h = hash(h, (uint32_t)dc_fault());
    if (ui.view == V_GARDEN) for (i = 0; i < EN_NPAD; i++) h = hash(h, en_chord[i]);
    if (ui.view == V_MIXER) {
        int vu[4]; mixer_vu_values(vu);
        h = hash(h, (uint32_t)(vu[0] | vu[1] << 8 | vu[2] << 16 | vu[3] << 24));
    }
    if(grid_page(ui.view)) {
        h=hash(h,(uint32_t)grid_row(ui.view));
        for(int row=1;row<=2;row++)for(int k=0;k<4;k++)
            h=hash(h,(uint32_t)knob_get(grid_binding(ui.view,row,k)));
    }
    h = hash(h, (uint32_t)now_touch);
    if (h == ui.sig[2]) return;
    ui.sig[2] = h;

    if (ui.view == V_SETUP) {
        /* GLO Setup: top label + ONLY eight setting boxes; two full-height rows. */
        const int32_t body_h=240-HDR_H;
        for(int32_t band=0;band<body_h;band+=102) {
            int32_t bh=body_h-band;if(bh>102)bh=102;
            cv_begin(240,(uint32_t)bh,K_BG);cv_oy=-band;
            if(ui.help) {
                cv_oy+=(body_h-KNB_H)/2;
                draw_help_row4(c);
            } else {
                for(int i=0;i<4;i++) {grid_cell(V_SETUP,1,i,4,96);grid_cell(V_SETUP,2,i,106,94);}
            }
            cv_blit(0,HDR_H+(uint32_t)band);
        }
        return;
    }
    if (ui.view == V_MIXER) {
        int vu[4]; mixer_vu_values(vu);
        const int32_t body_h = 240 - HDR_H;
        /* Use the existing small canvas in two bands; no larger framebuffer. */
        for (int32_t band = 0; band < body_h; band += 102) {
            int32_t band_h = body_h - band;
            if (band_h > 102) band_h = 102;
            cv_begin(240, (uint32_t)band_h, K_BG);
            cv_oy = -band;
            if (ui.help) {
                cv_oy += (body_h - KNB_H) / 2;
                draw_help_row4(c);
            } else {
                outline_box(1, 1, 238, body_h - 2, c, K_BG);
                draw_mixer_channel(4, 4, 56, body_h - 8, "PAD", proj.par[P_PAD], vu[0], K_PAD);
                draw_mixer_channel(63, 4, 56, body_h - 8, "RINGS", proj.par[P_BELLS], vu[1], K_BELL);
                draw_mixer_channel(122, 4, 56, body_h - 8, "PAD 2", proj.par[P_PULSE], vu[2], K_PULSE);
                draw_mixer_channel(181, 4, 55, body_h - 8, "FM", proj.par[P_FMLEVEL], vu[3], K_FM);
            }
            cv_blit(0, HDR_H + (uint32_t)band);
        }
        return;
    }
    cv_begin(240, KNB_H, K_BG);
    if (ui.help) {
        draw_help_row4(c);
        cv_blit(0, knob_y);
        return;
    }

    if(grid_page(ui.view)) {
        for(int i=0;i<4;i++)grid_cell(ui.view,2,i,4,66);
        cv_blit(0,knob_y);return;
    }
    if(ui.view==V_RUNITOPT || ui.view==V_FUNITOPT){
        for(int i=0;i<4;i++)unit_opt_cell(i,2,4,64);
        cv_blit(0,knob_y);return;
    }
    if (ui.view==V_FMFILTER) {
        for(int i=0;i<4;i++)fm_filter_cell(i,2,4,64);
        cv_blit(0,knob_y);return;
    }
    if(ui.view==V_MOD && ui.mod_route<11) {
        for(int i=0;i<4;i++)mod_shape_cell(i,2,4,66);
        cv_blit(0,knob_y);return;
    }
    if (ui.view==V_RINGSEQ || ui.view==V_FMSEQ) {
        for(int slot=0;slot<4;++slot)
            sequencer_cell(4+slot*59,4,64,slot,ui.view,2);
        cv_blit(0,knob_y);return;
    }
    if (rings_aux_grid_page()) {
        /* Continue the same boxes from the main band's lower edge. */
        cv_oy = -MAIN_H;
        draw_rings_aux_grid();
        cv_blit(0, knob_y);
        return;
    }
    outline_box(1, 1, 238, 70, c, K_BG);
    if (ui.view == V_GARDEN) {
        char chord[16], v[16];
        chord_now(chord);
        value_box(4, 4, 48, 64, "CHORD", chord, c, K_PANEL, 0);
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[ui.view][i];
            knob_text(k, v);
            value_box(54 + (int)i * 46, 4, 44, 64, KNOB_LABEL[ui.view][i], v, c, K_PANEL, now_touch && ui.touched == (int)i);
        }
    } else if (ui.view == V_MIXER) {
        int vu[4]; mixer_vu_values(vu);
        draw_mixer_channel(4, 4, 56, 64, "PAD", proj.par[P_PAD], vu[0], K_PAD);
        draw_mixer_channel(63, 4, 56, 64, "RINGS", proj.par[P_BELLS], vu[1], K_BELL);
        draw_mixer_channel(122, 4, 56, 64, "PAD 2", proj.par[P_PULSE], vu[2], K_PULSE);
        draw_mixer_channel(181, 4, 55, 64, "FM", proj.par[P_FMLEVEL], vu[3], K_FM);
    } else if (ui.view == V_PAD || ui.view == V_DRONE ||
               ui.view == V_RINGS || ui.view == V_FM || ui.view == V_MOD ||
               ui.view == V_RPROTOPT || ui.view == V_FPROTOPT) {
        /* Approved 2 by 2 grid, exactly four existing knob assignments. */
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[ui.view][i];
            char v[16];
            int32_t x = 4 + (int32_t)(i % 2) * 118;
            int32_t y = 4 + (int32_t)(i / 2) * 33;
            if (k == K_NONE) continue;
            knob_text(k, v);
            value_box_compact(x, y, 114, 31, KNOB_LABEL[ui.view][i], v, c,
                              K_PANEL, now_touch && ui.touched == (int)i);
        }
    } else {
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[ui.view][i];
            char v[16];
            int32_t x = 4 + (int32_t)i * 59;
            if (k == K_NONE) continue;
            knob_text(k, v);
            value_box(x, 4, 56, 64, KNOB_LABEL[ui.view][i], v, c, K_PANEL, now_touch && ui.touched == (int)i);
        }
    }
    cv_blit(0, knob_y);
}



static void leds(void)
{
    uint32_t b = (1u << NB) - 1u, k = 0, i, ap = pool_abs();
    /* The non-piano hardware buttons are lit by default. The button that owns
     * the current page is the inverse indicator: it goes fully dark while that
     * page (including its repeated-press subpages) is active. */
    b &= ~(1u << VIEW_BTN[ui.view]);
    /* PLAY/REC are transport-state LEDs, not part of the always-on page-button
     * field: dark while stopped/thawed, lit while playing/frozen. */
    if (en_playing) b |= 1u << B_PLAY; else b &= ~(1u << B_PLAY);
    if (en_frozen) b |= 1u << B_REC; else b &= ~(1u << B_REC);
    if (proj.par[P_MODE] == MODE_PIANO) {
        /* PIANO mode plays FM on its pages and Rings on other pages.
         * Illuminate every piano key regardless of the normal
         * scale-light preference. */
        k = (1u << NKEYS) - 1u;
    } else if (proj.leds) {
        for (i = 0; i < NKEYS; i++)
            if (ap >> ((5u + i) % 12u) & 1u)
                k |= 1u << i;
    }
    plat_glow(0);
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, while resting and quiet, nothing touched (a flash erase
 * silences the audio for a moment: never while the garden grows; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || en_pad_level > 0.01f || en_fm_level > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

/* ------------------------------------------------------ screen reader --- */
/* The screen as text for a screen reader on the computer (announce.c; fm1-reader): a page once it settles,
 * with its knobs; a knob's value as it turns; a help card as it comes up; the chord as it moves (queued: the
 * garden moves it by itself, every 20 s at the quickest); growing and resting from MIDI, and a MIDI clock
 * arriving or going; what a button did (say). WHERE (the reader asks, or it has just started listening) is
 * the whole screen in two: the page, its knobs and the header, then the garden (the chord, the scale).
 * Nothing is built while no reader listens. */
static struct {
    uint8_t view, help, ext, started;
    int16_t val[4];
    uint32_t chord;                            /* the chord's name, hashed */
} sp;
static char sp_buf[ANN_MAX + 1];
static uint32_t sp_n;
/* the chord names (en_chord_name's, after the root) in words: "m" and "M7" differ only in capitals, which
 * the words file ignores */
static const char *const CHORD_Q[][2] = {
    {"", "major"}, {"m", "minor"}, {"7", "7"}, {"M7", "major 7"}, {"m7", "minor 7"}, {"6", "6"},
    {"m6", "minor 6"}, {"add9", "add 9"}, {"madd9", "minor add 9"}, {"aug", "augmented"}, {"dim", "diminished"},
    {"dim7", "diminished 7"}, {"m7b5", "half diminished"}, {"mM7", "minor major 7"}, {"sus2", "suspended 2"},
    {"sus4", "suspended 4"}, {"7sus4", "7 suspended 4"}, {"5", "5"},
};
#define NCHORD_Q (sizeof CHORD_Q / sizeof CHORD_Q[0])

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX)
        sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static void sp_int(uint32_t v)
{
    char t[12];
    itoa_u(v, t);
    sp_cat(t);
}
static int sp_same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* a value as it is said: Drift in minutes and seconds (the screen's "1:05", "45s"), the rest as shown */
static void sp_value(int k)
{
    char t[16];
    knob_text(k, t);
    if (k == P_DRIFT) {
        uint32_t m = 0, s = 0, i;
        for (i = 0; t[i] >= '0' && t[i] <= '9'; i++)
            s = s * 10u + (uint32_t)(t[i] - '0');
        if (t[i] == ':') {
            m = s;
            s = 0;
            for (i++; t[i] >= '0' && t[i] <= '9'; i++)
                s = s * 10u + (uint32_t)(t[i] - '0');
        }
        if (m) {
            sp_int(m);
            sp_cat(m == 1u ? " minute" : " minutes");
            if (!s)
                return;
            sp_cat(" ");
        }
        sp_int(s);
        sp_cat(s == 1u ? " second" : " seconds");
        return;
    }
    sp_cat(t);
}
static void sp_knob(int view, int i)           /* "Swell 60": the name the page gives it, then its value */
{
    if (view==V_RINGSEQ || view==V_FMSEQ) {
        int zone=ui.seq_zone[view==V_FMSEQ];
        int k=seq_binding(view,zone,i);
        if(k==K_NONE)return;
        sp_cat(seq_binding_label(view,zone,i));sp_cat(" ");sp_value(k);
        return;
    }
    if(wide_grid_page(view) || view==V_MOD) {
        int k=bound_view_knob(view,i);if(k==K_NONE)return;
        sp_cat(wide_grid_page(view)?KNOB_LABEL[view][i]:k==K_MOD_SHAPE?"Shape":k==K_MOD_SPEED?"Speed":k==K_MOD_DEPTH?"Depth":k==K_MOD_DEST?"Destination":"Source");sp_cat(" ");sp_value(k);return;
    }
    if(grid_page(view)) {
        int row=grid_row(view),k=grid_binding(view,row,i);
        if(k==K_NONE)return;
        sp_cat(grid_label(view,row,i));sp_cat(" ");sp_value(k);
        return;
    }
    sp_cat(KNOB_LABEL[view][i]);
    sp_cat(" ");
    sp_value(VIEW_KNOB[view][i]);
}

static void sp_chord_q(int root, const char *q)   /* "E minor 7" */
{
    uint32_t i;
    sp_cat(EN_NOTE_NAME[root % 12]);
    sp_cat(" ");
    for (i = 0; i < NCHORD_Q; i++)
        if (sp_same(q, CHORD_Q[i][0])) {
            sp_cat(CHORD_Q[i][1]);
            return;
        }
    sp_cat(q);
}
static void sp_chord(void)                     /* the chord in the middle of the garden */
{
    char nm[16];
    const char *q = chord_now(nm), *r = EN_NOTE_NAME[en_chord_root % 12u];
    while (*r && *q == *r)
        q++, r++;
    sp_chord_q(en_chord_root % 12u, q);
}

/* the page: its name and its knobs; all: the header too (the piece, its key, the tempo, growing or resting) */
static void sp_page(int all)
{
    int i;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(VIEW_NAME[ui.view]);
    sp_cat(" page");
    {
        int first = 1;
        for (i = 0; i < 4; i++) {
            int k=(ui.view==V_RINGSEQ || ui.view==V_FMSEQ) ?
                seq_binding(ui.view,ui.seq_zone[ui.view==V_FMSEQ],i) : bound_view_knob(ui.view,i);
            if (k == K_NONE) continue;
            sp_cat(first ? ": " : ", ");
            sp_knob(ui.view, i);
            first = 0;
        }
    }
    if (!all)
        return;
    sp_cat(". Piece ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    if (en_ext) {
        sp_cat(", tempo EXT");
    } else if (ui.view != V_GARDEN) {          /* (the Garden's first knob says it) */
        sp_cat(", tempo ");
        sp_int((uint32_t)proj.par[P_TEMPO]);
    }
    sp_cat(sp_tp.playing ? ", growing" : ", resting");
    if (ui.dirty)
        sp_cat(", not saved yet");
    if (ui.view == V_SETUP) {
        sp_cat(". CPU ");
        sp_int(plat_cpu_pct());
        sp_cat("%");
    }
}

/* the garden: the chord and how far to the next, the scale from the root up ("F and F sharp": one letter's
 * two notes joined, so they are heard as two), the pulse sitting out */
static void sp_garden(void)
{
    uint32_t ap = pool_abs();
    int i, last = -1;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("Chord ");
    sp_chord();
    if (en_frozen) {
        sp_cat(", frozen");
    } else {
        sp_cat(", next chord ");
        sp_int((uint32_t)(fm_clampf(en_drift, 0.0f, 1.0f) * 100.0f + 0.5f));
        sp_cat("% of the way");
    }
    sp_cat(". Scale");
    for (i = 0; i < 12; i++) {
        int pc = (proj.par[P_ROOT] + i) % 12;
        if (ap >> pc & 1u) {
            sp_cat(last < 0 ? " " : EN_NOTE_NAME[last][0] == EN_NOTE_NAME[pc][0] ? " and " : ", ");
            sp_cat(EN_NOTE_NAME[pc]);
            last = pc;
        }
    }
    if (en_pulse_rest && proj.par[P_PULSE])
        sp_cat(". Pad 2 breathing");
}

/* a help card: its button, then its lines ("ENV button. THE PAD. LEVEL, SWELL, BRIGHT, COLOUR") */
static void sp_line(const char *s)
{
    char p = sp_n ? sp_buf[sp_n - 1] : 0;
    if (*s == ' ')                             /* it goes on from the line before */
        s++;
    else if (p != ',' && p != ':')
        sp_cat(".");
    sp_cat(" ");
    sp_cat(s);
}
static void sp_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(BTN_LABEL[ui.help - 1]);
    sp_cat(" button");
    for (k = 0; k < n; k++)
        sp_line(l[k]);
}

static void sp_piece_text(void)                /* "PIECE Departures, C# Ionian, tempo 60" */
{
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("PIECE ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    sp_cat(", tempo ");
    sp_int((uint32_t)proj.par[P_TEMPO]);
}
static void sp_piece(void)
{
    if (!ann_on())
        return;
    sp_piece_text();
    ann_say(sp_buf, 0, ANN_NOW);
    sp_said = 1;
}

static uint32_t sp_chord_sig(void)
{
    char nm[16];
    const char *s = chord_now(nm);
    uint32_t h = 2166136261u;
    while (*s)
        h = hash(h, (uint8_t)*s++);
    return h;
}
static void sp_snap(void)
{
    int i;
    for (i = 0; i < 4; i++)
        sp.val[i] = (int16_t)knob_get((ui.view==V_RINGSEQ || ui.view==V_FMSEQ) ?
            seq_binding(ui.view,ui.seq_zone[ui.view==V_FMSEQ],i) : bound_view_knob(ui.view,i));
    sp.view = ui.view;
    sp.help = ui.help;
    sp.ext = en_ext;
    sp.chord = sp_chord_sig();
}

static void speak(void)
{
    uint32_t now = plat_ms(), c;
    int where = ann_where(), i, changed = -1;
    if (!ann_on()) {
        sp.started = 0;
        sp_said = 0;
        return;
    }
    if (!sp.started || where) {                       /* everything on the screen, in two */
        sp.started = 1;
        sp_snap();
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            sp_page(1);
        } else {
            sp_page(1);
            ann_say(sp_buf, 0, ANN_NOW);
            sp_garden();
        }
        ann_say(sp_buf, 0, 0);
        sp_said = 0;
        return;
    }
    if (sp_said) {                                    /* a button said what it did (and what it changed) */
        sp_said = 0;
        sp_snap();
        return;
    }
    if (en_playing != sp_tp.playing && now - sp_tp.play_t > 150u) {   /* MIDI start / stop */
        sp_tp.playing = en_playing;
        ann_say(en_playing ? "GROWING" : "RESTING", 0, 0);
    }
    if (en_ext != sp.ext) {                           /* a MIDI clock arrived, or stopped */
        sp.ext = en_ext;
        sp_n = 0;
        sp_cat("Tempo ");
        if (en_ext)
            sp_cat("EXT");
        else
            sp_int((uint32_t)proj.par[P_TEMPO]);
        ann_say(sp_buf, 0, 0);
    }
    if (ui.help != sp.help) {                         /* a help card: at once */
        sp.help = ui.help;
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            return;
        }
    }
    if (ui.view != sp.view) {                         /* a new page: said when it settles */
        sp_snap();
        sp_page(0);
        ann_later(sp_buf, ANN_NOW, 200u, now);
        return;
    }
    for (i = 0; i < 4; i++) {                         /* a knob turning */
        int v = knob_get((ui.view==V_RINGSEQ || ui.view==V_FMSEQ) ?
            seq_binding(ui.view,ui.seq_zone[ui.view==V_FMSEQ],i) : bound_view_knob(ui.view,i));
        if (v != sp.val[i]) {
            sp.val[i] = (int16_t)v;
            if (changed < 0)
                changed = i;
        }
    }
    if (changed >= 0) {
        sp_n = 0;
        sp_knob(ui.view, changed);
        ann_later(sp_buf, ANN_NOW, 120u, now);
    }
    c = sp_chord_sig();
    if (c != sp.chord) {                              /* the chord moved: queued, after what is being said */
        sp.chord = c;
        sp_n = 0;
        sp_cat("Chord ");
        sp_chord();
        ann_say(sp_buf, 0, 0);
    }
}

#ifdef PLAT_HOST
/* every page, knob value, help card, chord and message as a screen reader gets them (the "speakdump"
 * command): KIND <tab> the text sent (fm1-reader/tools/audit.mjs puts it in words with web/words/ and checks it) */
static void sp_print(const char *kind, const char *t) { printf("%s\t%s\n", kind, t); }
static void speak_dump(void)
{
    static const char *const MSG[][2] = {{"UPDATE MODE IN", "3.."}, {"UPDATE MODE", "CANCELLED"},
                                         {"NEXT CHORD", "ON THE BAR"}, {"NEXT CHORD", 0}, {"RESTING", 0},
                                         {"GROWING", 0}, {"FREEZE", "ON"}, {"FREEZE", "OFF"}, {"SAVED", 0}};
    static const char *const WARN[][2] = {{"THE ROOT STAYS", "(HOME)"}, {"3 NOTES", "AT LEAST"},
                                          {"NOT SAVED", "FLASH ERROR"}};
    project_t keep = proj;
    uint8_t keep_view = ui.view, keep_help = ui.help, keep_dirty = ui.dirty, keep_play = sp_tp.playing;
    uint8_t keep_ext = en_ext, keep_frozen = en_frozen, keep_rest = en_pulse_rest;
    int v, i, x, lo, hi;
    char t[48];
    for (v = 0; v < NVIEWS; v++) {
        ui.view = (uint8_t)v;
    if(v==V_FMFILTER || v==V_PAD2FILTER)ui.fm_filter_row=1;
        sp_page(0);
        sp_print("SCREEN", sp_buf);
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[v][i];
            knob_range(k, &lo, &hi);
            if (k == P_RMODEL) { lo = 0; hi = RINGS_MODEL_COUNT - 1; }
            for (x = lo; x <= hi; x++) {
                if (k < P_NPARAMS)
                    proj.par[k] = (int16_t)(k == P_RMODEL ? rings_model_from_index(x) : x);
                else
                    proj.leds = (uint8_t)x;
                sp_n = 0;
                sp_knob(v, i);
                sp_print("KNOB", sp_buf);
            }
            proj = keep;
        }
    }
    for (x = 0; x < 8; x++) {                         /* WHERE: growing, resting, unsaved, a clock, frozen, custom */
        ui.view = (uint8_t)(x * 3 % NVIEWS);
        sp_tp.playing = (uint8_t)(x & 1);
        ui.dirty = (uint8_t)(x >> 1 & 1);
        en_ext = (uint8_t)(x == 3);
        en_frozen = (uint8_t)(x == 5);
        en_pulse_rest = (uint8_t)(x == 2);
        proj.pool = x == 6 ? 0xFFFu : x == 7 ? 0x91u : keep.pool;
        proj.par[P_ROOT] = (int16_t)(x * 5 % 12);
        sp_page(1);
        sp_print("WHERE", sp_buf);
        sp_garden();
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    ui.dirty = keep_dirty;
    sp_tp.playing = keep_play;
    en_ext = keep_ext;
    en_frozen = keep_frozen;
    en_pulse_rest = keep_rest;
    ui.view = keep_view;
    for (i = 0; i < NB; i++) {
        const char *l[HELP_LINES];
        if (!help_lines(i, l))
            continue;
        ui.help = (uint8_t)(i + 1);
        sp_help();
        sp_print("HELP", sp_buf);
    }
    ui.help = keep_help;
    for (x = 0; x < 12; x++)
        for (i = 0; i < (int)NCHORD_Q; i++) {
            sp_n = 0;
            sp_cat("Chord ");
            sp_chord_q(x, CHORD_Q[i][0]);
            sp_print("CHORD", sp_buf);
        }
    for (i = 0; i < (int)(sizeof MSG / sizeof MSG[0]); i++) {
        msg_text(MSG[i][0], MSG[i][1], t);
        sp_print("MSG", t);
    }
    for (i = 0; i < (int)(sizeof WARN / sizeof WARN[0]); i++) {
        msg_text(WARN[i][0], WARN[i][1], t);
        sp_print("WARN", t);
    }
    for (x = 0; x < 12; x++) {
        msg_text("ADDED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
        msg_text("REMOVED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = 0; x < EN_NMODES; x++) {
        msg_text("MODE", EN_MODE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = -4; x <= 2; x++) {
        char o[8];
        en_param_text(P_BOCT, x, o);
        msg_text("RINGS OCTAVE", o, t);
        sp_print("MSG", t);
        en_param_text(P_FMOCT, x, o);
        msg_text("FM OCTAVE", o, t);
        sp_print("MSG", t);
        en_param_text(P_PADOCT, x, o);
        msg_text("PAD OCTAVE", o, t);
        sp_print("MSG", t);
        en_param_text(P_PAD2OCT, x, o);
        msg_text("PAD 2 OCTAVE", o, t);
        sp_print("MSG", t);
    }
    for (x = 30; x <= 120; x += 45) {
        char o[8];
        itoa_u((uint32_t)x, o);
        msg_text("TEMPO", o, t);
        sp_print("MSG", t);
    }
    for (x = 0; x < NPIECES; x++) {
        project_piece(x);
        sp_piece_text();
        sp_print("MSG", sp_buf);
        msg_text("RESET", piece_name(x), t);
        sp_print("MSG", t);
    }
    proj = keep;
    sp_print("MSG", "Tempo EXT");
    sp_print("MSG", "Tempo 72");
}
#endif

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.unit_row[0]=ui.unit_row[1]=1;
    ui.mod_row=1; ui.fm_filter_row=1;
    memset(fm_key_note, 0, sizeof fm_key_note);
    memset(fm_midi_note, 0, sizeof fm_midi_note);
    ui.touched = -1;
    lcd_fill(0, 0, 240, 240, K_BG);
    if (!en_playing)
        en_play(1);                              /* it grows from the start */
    sp_tp.playing = 1;
    sp_tp.play_t = plat_ms();
}


#include "ui_dusk.h"

void ui_frame(void)
{
    input();
    autosave();
    dusk_draw();
    leds();
    speak();
    ann_tick(plat_ms());
    ui.frame++;
}

void ui_input_only(void) { input(); }
