/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 on fm1-base: the app's unity build (compiled into fm1-base's, after the platform) and the glue between
 * fm1-base's contract (app.h) and AMB-1's parts: the project (project.c), the UI (ui.c) and the engine (dsp/amb.c,
 * its own -O2 unit). */
#include "project.c"
#include "plat.h"
#include "ui.c"
#include "splash_genamb.h"

/* 240x240 indexed splash. The shared canvas is reused in 24-row strips, so the
 * artwork costs no extra RAM: 58 KB lives in XIP flash and only the existing
 * screen canvas is touched while the device is starting. */
void app_splash(void)
{
    uint32_t y, i;
    for (y = 0; y < 240u; y += 24u) {
        uint32_t h = 240u - y;
        if (h > 24u) h = 24u;
        cv_begin(240u, h, C_BLACK);
        for (i = 0; i < 240u * h; i++) {
            uint32_t k = GENAMB_SPLASH_IDX[y * 240u + i];
            cv_px[i] = swap16(GENAMB_SPLASH_PAL[k]);
        }
        cv_blit(0, y);
    }
}

/* the engine's MIDI out (clock, notes when MIDI out is on): to USB */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

/* ---- the contract */
#if defined(PLAT_HOST) || defined(EN_HOST)
#define BOOT_STAGE(n) ((void)0)
#else
extern volatile uint32_t fm1_startup_stage;
#define BOOT_STAGE(n) (fm1_startup_stage = (n))
#endif
void app_init(void)
{
    BOOT_STAGE(10);
    en_init();
    BOOT_STAGE(20);
    project_load();                      /* defaults when nothing (or another format) is saved */
    en_evo_restore_boot(&proj.evo);
    en_fmevo_restore_boot(&proj.fm_evo);
    en_proteus_restore_boot(&proj.proteus);
    en_unitd_restore_boot(&proj.unitd);
    BOOT_STAGE(30);
    project_apply();
    BOOT_STAGE(40);
}
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12) { en_render(lr, n, gain_q12); }
void app_panic(void)
{
    en_play(0);
    en_panic();
}
void app_ui_init(void) { ui_init(); }
void app_frame(void) { ui_frame(); }
void app_input_only(void) { ui_input_only(); }
void app_say(const char *a, const char *b) { ui_say(a, b); }


/* -------------------------------------------------------------------------
 * genAMB USB-C Web MIDI control protocol.
 *
 * Frames use the non-commercial SysEx ID 0x7D and the signature "GA":
 *   F0 7D 'G' 'A' 01 <command> ... F7
 * app_sysex() receives the bytes between F0/F7.
 *
 * All 167 engine parameters use the exact P_* IDs from amb.h. Signed values
 * are carried as 14-bit unsigned values biased by 8192 so every data byte
 * remains MIDI-safe. State is returned in short parameter chunks to stay well
 * inside the FM-1 USB-MIDI SysEx queues.
 */
#define GA_PROTO 1u
#define GA_PARAM_CHUNK 32u
#define GA_S14_BIAS 8192

enum {
    GA_HELLO = 0x01,
    GA_GET_STATE = 0x02,
    GA_SET_PARAM = 0x03,
    GA_SET_PARAM_BLOCK = 0x04,
    GA_SET_META = 0x05,
    GA_LOAD_PIECE = 0x06,
    GA_ACTION = 0x07,

    GA_R_HELLO = 0x41,
    GA_R_META = 0x42,
    GA_R_PARAMS = 0x43,
    GA_R_ACK = 0x45
};

enum {
    GA_ACT_PLAY_TOGGLE = 0,
    GA_ACT_PLAY_ON,
    GA_ACT_PLAY_OFF,
    GA_ACT_FREEZE_TOGGLE,
    GA_ACT_FREEZE_ON,
    GA_ACT_FREEZE_OFF,
    GA_ACT_NEXT_CHORD,
    GA_ACT_REGROW,
    GA_ACT_RESET_PIECE,
    GA_ACT_SAVE_PROJECT,
    GA_ACT_PANIC,
    GA_ACT_RINGS_EVOLVE_NEW,
    GA_ACT_FM_EVOLVE_NEW,
    GA_ACT_RINGS_PROTEUS_NEW,
    GA_ACT_FM_PROTEUS_NEW,
    GA_ACT_PERF_RESET
};

static uint8_t ga_tx[80];
static uint8_t ga_connected;

static unsigned ga_put_u14(uint8_t *d, unsigned v)
{
    if (v > 16383u) v = 16383u;
    d[0] = (uint8_t)(v & 0x7fu);
    d[1] = (uint8_t)((v >> 7) & 0x7fu);
    return 2u;
}
static unsigned ga_get_u14(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 7); }
static unsigned ga_put_s14(uint8_t *d, int v)
{
    if (v < -8192) v = -8192;
    if (v > 8191) v = 8191;
    return ga_put_u14(d, (unsigned)(v + GA_S14_BIAS));
}
static int ga_get_s14(const uint8_t *p) { return (int)ga_get_u14(p) - GA_S14_BIAS; }

static unsigned ga_begin(unsigned cmd)
{
    ga_tx[0] = 0xf0u; ga_tx[1] = 0x7du; ga_tx[2] = 'G'; ga_tx[3] = 'A';
    ga_tx[4] = GA_PROTO; ga_tx[5] = (uint8_t)cmd;
    return 6u;
}
static void ga_finish(unsigned n)
{
    if (n < sizeof ga_tx - 1u) { ga_tx[n++] = 0xf7u; plat_sysex_send(ga_tx, n); }
}
static void ga_ack(unsigned command, unsigned status)
{
    unsigned n = ga_begin(GA_R_ACK);
    ga_tx[n++] = (uint8_t)(command & 0x7fu);
    ga_tx[n++] = (uint8_t)(status & 0x7fu);
    ga_finish(n);
}
static void ga_send_hello(void)
{
    unsigned n = ga_begin(GA_R_HELLO);
    ga_tx[n++] = 2u; /* WEB1 feature level */
    n += ga_put_u14(ga_tx + n, P_NPARAMS);
    ga_tx[n++] = (uint8_t)NPIECES;
    n += ga_put_u14(ga_tx + n, PROJ_FORMAT);
    ga_finish(n);
}
static void ga_send_meta(void)
{
    unsigned n = ga_begin(GA_R_META);
    ga_tx[n++] = proj.piece;
    ga_tx[n++] = proj.leds;
    ga_tx[n++] = en_playing ? 1u : 0u;
    ga_tx[n++] = en_frozen ? 1u : 0u;
    n += ga_put_u14(ga_tx + n, proj.pool);
    ga_finish(n);
}
static void ga_send_param_chunk(unsigned start)
{
    unsigned n = ga_begin(GA_R_PARAMS), i, count = P_NPARAMS - start;
    if (count > GA_PARAM_CHUNK) count = GA_PARAM_CHUNK;
    n += ga_put_u14(ga_tx + n, start);
    ga_tx[n++] = (uint8_t)count;
    for (i = 0; i < count; ++i) n += ga_put_s14(ga_tx + n, proj.par[start + i]);
    ga_finish(n);
}
static void ga_send_state(void)
{
    unsigned p;
    ga_send_meta();
    for (p = 0; p < P_NPARAMS; p += GA_PARAM_CHUNK) ga_send_param_chunk(p);
}

static int ga_set_meta(unsigned piece, unsigned leds, unsigned playing, unsigned frozen, unsigned pool)
{
    if (piece >= NPIECES || leds > 2u || !pool_ok(pool)) return 0;
    proj.piece = (uint8_t)piece;       /* snapshot identity only: do not load/overwrite another piece slot */
    proj.leds = (uint8_t)leds;
    proj.pool = (uint16_t)pool;
    en_pool(pool);
    en_play(playing != 0u);
    en_freeze(frozen != 0u);
    mark_dirty();
    return 1;
}
static int ga_action(unsigned a)
{
    switch (a) {
    case GA_ACT_PLAY_TOGGLE: button(B_PLAY); break;
    case GA_ACT_PLAY_ON: if (!en_playing) button(B_PLAY); break;
    case GA_ACT_PLAY_OFF: if (en_playing) button(B_PLAY); break;
    case GA_ACT_FREEZE_TOGGLE: button(B_REC); break;
    case GA_ACT_FREEZE_ON: if (!en_frozen) button(B_REC); break;
    case GA_ACT_FREEZE_OFF: if (en_frozen) button(B_REC); break;
    case GA_ACT_NEXT_CHORD: button(B_EDIT); break;
    case GA_ACT_REGROW: en_seed(plat_ms()); en_regrow(); mark_dirty(); say("REGROW", "NEW GARDEN"); break;
    case GA_ACT_RESET_PIECE:
        fm_manual_release(); project_piece_reset(); project_apply(); en_regrow(); mark_dirty(); say("RESET", piece_name(proj.piece)); break;
    case GA_ACT_SAVE_PROJECT: button(B_SAVE); break;
    case GA_ACT_PANIC: app_panic(); break;
    case GA_ACT_RINGS_EVOLVE_NEW: en_evo_new(); mark_dirty(); say("PATTERN", "RANDOMIZED"); break;
    case GA_ACT_FM_EVOLVE_NEW: en_fmevo_new(); mark_dirty(); say("FM PATTERN", "RANDOMIZED"); break;
    case GA_ACT_RINGS_PROTEUS_NEW: en_proteus_new(0); mark_dirty(); say("PROTEUS", "NEW MELODY"); break;
    case GA_ACT_FM_PROTEUS_NEW: en_proteus_new(1); mark_dirty(); say("FM PROTEUS", "NEW MELODY"); break;
    case GA_ACT_PERF_RESET: perf_reset(); break;
    default: return 0;
    }
    return 1;
}

int app_sysex(const uint8_t *p, uint32_t n)
{
    unsigned cmd;
    if (!p || n < 5u || p[0] != 0x7du || p[1] != 'G' || p[2] != 'A' || p[3] != GA_PROTO) return 0;
    cmd = p[4]; ga_connected = 1u;
    if (cmd == GA_HELLO) { ga_send_hello(); ga_send_state(); return 1; }
    if (cmd == GA_GET_STATE) { ga_send_state(); return 1; }
    if (cmd == GA_SET_PARAM) {
        unsigned id; int v;
        if (n < 9u) { ga_ack(cmd, 2); return 1; }
        id = ga_get_u14(p + 5); v = ga_get_s14(p + 7);
        if (id >= P_NPARAMS) { ga_ack(cmd, 3); return 1; }
        knob_set((int)id, v); ga_ack(cmd, 0); return 1;
    }
    if (cmd == GA_SET_PARAM_BLOCK) {
        unsigned start, count, i;
        if (n < 8u) { ga_ack(cmd, 2); return 1; }
        start = ga_get_u14(p + 5); count = p[7];
        if (start >= P_NPARAMS || count > GA_PARAM_CHUNK || start + count > P_NPARAMS || n < 8u + count * 2u) { ga_ack(cmd, 3); return 1; }
        /* Snapshot restore is deliberately literal: unlike one-at-a-time UI
         * edits, a block must not trigger model-selection convenience defaults
         * (for example GP5 model changing decay/tone). The .genamb file is an
         * exact snapshot, and GA_SET_META restores its custom scale afterwards. */
        for (i = 0; i < count; ++i) {
            unsigned id = start + i; int v = ga_get_s14(p + 8u + i * 2u);
            const en_param_t *pi = en_param_info((int)id);
            if (v < pi->lo || v > pi->hi) { ga_ack(cmd, 4); return 1; }
        }
        for (i = 0; i < count; ++i) {
            unsigned id = start + i; int v = ga_get_s14(p + 8u + i * 2u);
            proj.par[id] = (int16_t)v; en_set((int)id, v);
        }
        mark_dirty(); ga_ack(cmd, 0); return 1;
    }
    if (cmd == GA_SET_META) {
        unsigned piece, leds, playing, frozen, pool;
        if (n < 11u) { ga_ack(cmd, 2); return 1; }
        piece = p[5]; leds = p[6]; playing = p[7]; frozen = p[8]; pool = ga_get_u14(p + 9);
        ga_ack(cmd, ga_set_meta(piece, leds, playing, frozen, pool) ? 0u : 3u); return 1;
    }
    if (cmd == GA_LOAD_PIECE) {
        if (n < 6u || p[5] >= NPIECES) { ga_ack(cmd, 3); return 1; }
        set_piece(p[5]); ga_send_state(); ga_ack(cmd, 0); return 1;
    }
    if (cmd == GA_ACTION) {
        if (n < 6u || !ga_action(p[5])) { ga_ack(cmd, 3); return 1; }
        ga_send_meta(); ga_ack(cmd, 0); return 1;
    }
    ga_ack(cmd, 5); return 1;
}

#ifdef PLAT_HOST
#include "host.c"                        /* the host simulator's part: checks and commands */
#endif

#if defined(PLAT_HOST)
/* UI preview only: no hardware load is fabricated on the host. */
#include "perf.h"
uint32_t host_perf_dual=1;
static uint32_t host_perf_start;
void perf_read(perf_stats_t *s) {
    memset(s,0,sizeof *s); s->dual=host_perf_dual; s->status=2;
    s->seconds=(plat_ms()-host_perf_start)/1000u;
}
void perf_reset(void) { host_perf_start=plat_ms(); }
void perf_mode(int dual) { host_perf_dual=dual!=0; perf_reset(); }
#endif
