/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument: a generative ambient garden. Four layers draw on one pool of notes (a scale:
 * the root plus up to eleven more pitch classes) and one chord that drifts slowly through it:
 *
 *   PAD     four breathing voices: each swells in, hangs and fades on its own cycle (unequal lengths,
 *           as the loops of Music for Airports), taking its note of the chord as it starts, so the
 *           chord forms and dissolves. Two colours: a filtered Rhodes (FM, blooming brighter as it swells) or a warm
 *           vintage analog polysynth voice (detuned saw/triangle VCOs into a low-pass); then a soft low-pass and an
 *           ensemble chorus. The chord moves every DRIFT (20 s .. 5 min): voices on shared
 *           notes hold, the others breathe in anew.
 *   BELLS   soft two-operator FM bells played by four tape loops of unequal length (13, 17, 23, 29
 *           quarter notes), each holding a short phrase that shifts against the others, each its own
 *           shade of bell. EVOLVE changes a note now and then; DENSITY sets how many notes a loop holds.
 *   PULSE   a soft thump, a closed hat and an open hat, each on its own Euclidean pattern (16, 12 and 7
 *           sixteenths): polyrhythms. HITS sets how many; the patterns turn and change around it.
 *
 * Over it all, the ARC: a slow tide, minutes long, between sparse (fewer bells, a darker vowel, a quieter
 * pulse, more reverb) and full; MOTION sets how far it goes. And rests: with EVOLVE, the pulse
 * sits out a few bars, the bass a chord, a bell loop a pass.
 * Then, after Discreet Music's chain, an Echoplex (160 ms, fed back) and a tape loop (to ~5.6 s, two
 * heads, each repeat darker), and Dattorro's plate (from FoMni), fed generously: the space is most of
 * what is heard.
 * FREEZE holds the chord and lets the plate ring on forever.
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NLOOP 4                     /* bell tape loops */
#define EN_LOOP_MAX 29
#define EN_NDRUM 3                     /* pulse: thump, closed hat, open hat */
#define EN_NPAD 4                      /* pad voices */

/* modes: pools relative to the root (bit i: root + i semitones) */
enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_PENT_MAJ,
       MODE_PENT_MIN, MODE_PIANO, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern const uint8_t EN_LOOP_LEN[EN_NLOOP];
extern const uint8_t EN_DRUM_N[EN_NDRUM];

/* parameters: value ranges in en_param_info() */
enum {
    P_PAD, P_BELLS, P_P2FILTERMODEL, P_PULSE,                  /* layer levels */
    P_DENSITY, P_EVOLVE, P_DRIFT, P_HITS,             /* motion */
    P_ECHO, P_ETIME, P_REVERB, P_SIZE,                /* space */
    P_BRIGHT, P_SWELL, P_BTONE, P_MOTION,             /* pad tone/body + motion */
    P_TUNE, P_MIDI, P_TEMPO, P_ROOT, P_MODE, P_BOCT,  /* setup, panel encoders, bell octave */
    P_PADTONE,                                        /* pad colour: Rhodes, Vintage Analog */
    P_P2FILTERTYPE, P_P2FILTERCUT, P_P2FILTERRES,                      /* bass: its octave, its glide, how often it leans on the fifth */
    P_SWING, P_HATS,                                  /* pulse: swing, the hats' colour */
    P_RMODEL, P_RSTRUCT, P_RBRIGHT, P_RDAMP, P_RPOS, P_RPOLY, P_RFX,
    P_M1DEPTH, P_M1SPEED, P_M2DEPTH, P_M2SPEED, P_M3DEPTH, P_M3SPEED,
    P_M4DEPTH, P_M4SPEED, P_M5DEPTH, P_M5SPEED, P_M6DEPTH, P_M6SPEED,
    P_M7DEST, P_M7DEPTH, P_M7SPEED,
    P_M8DEST, P_M8DEPTH, P_M8SPEED,
    P_M9DEST, P_M9DEPTH, P_M9SPEED,
    P_PADCHAR, P_PADREV, P_RINGREV, P_DRONEREV, P_P2FILTERMIX,
    P_DRONEVOICE, P_REXCITER, P_PADCHORUS,
    P_PADOCT,                                         /* pad octave: -4..+2, appended for project compatibility */
    P_PAD2OCT, P_PAD2CHAR, P_PAD2CHORUS,              /* Pad 2: independent duplicate of Pad, appended for compatibility */
    P_RDELAYTIME, P_RDELAYFB, P_RDELAYMIX, P_RDELAYREPEATS, /* Rings-only mini delay; repeats 0 = infinite */
    P_RBURST,                                         /* Rings generative strum probability, 0..100% */
    P_PADRANGE, P_PADCHANCE, P_PAD2RANGE, P_PAD2CHANCE, P_RINGRANGE, P_RINGCHANCE,
    P_REVSPACE, P_DDIFF, P_DMOTION, P_DREV, /* ambient effects, appended for migration */
    P_RSEQMODE, P_ELENGTH, P_EMUTATE, P_EDENSITY, P_ERATE, P_ERANGE, P_EFOLLOW, P_EPATTERN,
    P_FMLEVEL, P_FMPATCH, P_FMPOLY, P_FMGATE, P_FMREV,
    P_FMSEQMODE, P_FMOCT, P_FMRANGE, P_FMCHANCE, P_FMDENSITY, P_FMBURST,
    P_FMELENGTH, P_FMEMUTATE, P_FMEDENSITY, P_FMERATE, P_FMERANGE, P_FMEFOLLOW, P_FMEPATTERN,
    /* v19: real Proteus melody engine; separate Rings and FM controls. Append only. */
    P_RPROTLEN, P_RPROTDENS, P_RPROTPATIENCE, P_RPROTRATE,
    P_RPROTGATE, P_RPROTOCT, P_RPROTMUTATE, P_RPROTLOCK, P_RPROTRANGE,
    P_FPROTLEN, P_FPROTDENS, P_FPROTPATIENCE, P_FPROTRATE,
    P_FPROTGATE, P_FPROTOCT, P_FPROTMUTATE, P_FPROTLOCK, P_FPROTRANGE,
    /* v20, append only: 10 FM voices, post-FM modelled filter and 11 mod sources */
    P_FFILTERMODEL, P_FFILTERTYPE, P_FFILTERCUT, P_FFILTERRES, P_FFILTERMIX,
    P_M7SHAPE, P_M8SHAPE, P_M9SHAPE,
    P_M10DEPTH, P_M10SPEED, P_M10SHAPE,
    P_M11DEPTH, P_M11SPEED, P_M11SHAPE,
    /* v21: two independent UNIT-D graph sequencers, 12 controls each. */
    P_RUDSEED, P_RUDNODES, P_RUDRADIUS, P_RUDDENS,
    P_RUDWALK, P_RUDLOCK, P_RUDGATE, P_RUDGDEN,
    P_RUDTOL, P_RUDRANGE, P_RUDRATE, P_RUDPOLY,
    P_FUDSEED, P_FUDNODES, P_FUDRADIUS, P_FUDDENS,
    P_FUDWALK, P_FUDLOCK, P_FUDGATE, P_FUDGDEN,
    P_FUDTOL, P_FUDRANGE, P_FUDRATE, P_FUDPOLY,
    /* Full PolyWavetable GP5 reverb controls, append-only for project compatibility. */
    P_GP5TONE, P_GP5RATE,
    P_NPARAMS
};
#define P_BASS P_P2FILTERMODEL /* removed Bass ID, retained for historical migrations */
#define P_BASSOCT P_P2FILTERTYPE /* removed Bass ID, retained for historical migrations */
#define P_GLIDE P_P2FILTERCUT /* removed Bass ID, retained for historical migrations */
#define P_FIFTH P_P2FILTERRES /* removed Bass ID, retained for historical migrations */
#define P_BASSREV P_P2FILTERMIX /* removed Bass ID, retained for historical migrations */

typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
void en_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_pool(uint32_t pool);                   /* the scale, relative to the root (bit 0 always set) */
void en_play(int on);                          /* the garden grows / rests (the tails ring on) */
void en_next(void);                            /* the next chord: on the next bar (now while resting) */
void en_freeze(int on);
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_regrow(void);                          /* a new piece: fresh loops, the home chord */
void en_rings_note(int midi_note, int velocity); /* manual Rings strike (PIANO mode / UI) */
void en_fm_note(int midi_note, int velocity);   /* held until matching note_off */
void en_fm_note_off(int midi_note);
void en_fm_all_notes_off(void);
/* MIDI clock in (as FoMni): ticks drive the 1/16 grid (6 a step) while they keep coming */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* state for the screen (UI) */
#define EN_MOD_TRACE 224
extern volatile int8_t en_mod_trace[11][EN_MOD_TRACE];
extern volatile uint16_t en_mod_trace_head, en_mod_trace_count;
extern volatile uint8_t en_playing, en_frozen, en_ext;
extern volatile uint8_t en_chord[EN_NPAD];     /* the chord's notes (MIDI), pad voices' targets */
extern volatile uint8_t en_chord_root;         /* the chord's root, MIDI pitch class 0..11 */
extern volatile uint8_t en_chord_n;            /* its distinct tones: 3 or 4 */
extern volatile float en_drift;                /* 0..1: how far to the next chord */
extern volatile float en_pad_level, en_bass_level, en_drone_level, en_reverb_level;
extern volatile float en_fm_level;
extern volatile uint8_t en_fm_loop_pos[EN_NLOOP];
extern volatile int8_t en_fm_loop_note[EN_NLOOP][EN_LOOP_MAX];
extern volatile float en_fm_loop_glow[EN_NLOOP];
extern volatile uint8_t en_loop_pos[EN_NLOOP];
extern volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];   /* -1 rest, else the slot's offset */
extern volatile float en_loop_glow[EN_NLOOP];  /* 0..1: a bell just rang from this loop */
extern volatile uint8_t en_drum_pos[EN_NDRUM];
extern volatile uint32_t en_drum_pat[EN_NDRUM];/* bit i: a hit on step i */
extern volatile float en_drum_glow[EN_NDRUM];
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */
extern volatile float en_arc;                  /* the arc, -1 sparse .. +1 full (times Motion) */
extern volatile uint8_t en_pulse_rest;         /* the pulse is sitting a few bars out */
extern volatile uint8_t en_pad_note[EN_NPAD];  /* the note each voice sings now (MIDI) */
extern volatile float en_pad_amp[EN_NPAD];     /* and how loud */

/* the pure parts, for the tests and the screen */
uint32_t en_euclid(int k, int n, int rot);     /* Bjorklund's pattern, as a bitmask (bit 0 = step 0) */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
/* the chord on scale degree deg: its tones (MIDI pitch classes, absolute), 3 or 4 of them; count */
int en_chord_tones(uint32_t pool, int root, int deg, int *pcs);
/* voice leading: new pad notes for the tones, each voice moving as little as possible, all in lo..hi */
void en_voice_lead(const int *from, const int *pcs, int ntones, int *to, int lo, int hi);
const char *en_chord_name(const int *pcs, int n, char *buf);   /* "Em7", "Dsus4", "C" ... */

/* the pieces: recipes for every parameter (Tune and MIDI out included, but they are the player's) */
#define EN_NPIECES 8
const char *en_piece_name(int i);
void en_piece(int i, int16_t *par, uint16_t *pool);

/* Compact Evolve bank, shared by the eight pieces. Each byte is gate:1/value:7. */
typedef struct { uint32_t magic, rng; uint8_t step[16][16]; } en_evo_bank_t;
/* A serialized Proteus melody per engine, not additional synth/audio voices.
 * Relative chord-tone ranks allow perfect live following of the Original chord. */
typedef struct {
    uint32_t rng;
    uint8_t note[32], restorder[32];
    uint8_t length, pos, repeats;
    int8_t octave_shift;
} en_proteus_state_t;
typedef struct { uint32_t magic; en_proteus_state_t seq[2]; } en_proteus_bank_t;
extern volatile uint8_t en_prot_pos[2], en_ring_seq_active_mode, en_fm_seq_active_mode;
extern volatile uint8_t en_prot_trace[2][32];
void en_proteus_snapshot(en_proteus_bank_t *out);
void en_proteus_restore_boot(const en_proteus_bank_t *in);
void en_proteus_new(int layer);
#ifdef PLAT_HOST
extern volatile uint32_t prot_note_count[2], prot_chord_failure[2];
#endif
extern volatile uint8_t en_evo_active, en_evo_pattern, en_evo_pos;
extern volatile uint8_t en_evo_trace[16];
void en_evo_new(void);
void en_evo_snapshot(en_evo_bank_t *out);
void en_evo_restore_boot(const en_evo_bank_t *in); /* before audio starts only */
extern volatile uint8_t en_fm_evo_active, en_fm_evo_pattern, en_fm_evo_pos;
extern volatile uint8_t en_fm_evo_trace[16];
void en_fmevo_new(void);
void en_fmevo_snapshot(en_evo_bank_t *out);
void en_fmevo_restore_boot(const en_evo_bank_t *in);

/* Native UNIT-D graph state is fixed-size and stored in audio pool. */
typedef struct { uint32_t walk,step; uint8_t node[8]; } en_ud_save_t;
typedef struct { uint32_t magic; en_ud_save_t seq[2]; } en_ud_bank_t;
void en_unitd_snapshot(en_ud_bank_t *out);
void en_unitd_restore_boot(const en_ud_bank_t *in);
extern volatile uint32_t ud_note_count[2],ud_chord_fail[2];

/* Bounded display-only UNIT-D graph snapshot. Published without allocating a framebuffer. */
#define EN_UD_GRAPH_N 64
#define EN_UD_GRAPH_POLY 8
typedef struct {
    uint8_t count,poly,x[EN_UD_GRAPH_N],y[EN_UD_GRAPH_N],active[EN_UD_GRAPH_POLY];
    uint64_t edges[EN_UD_GRAPH_N];
} en_unitd_screen_t;
void en_unitd_read(int layer,en_unitd_screen_t *out);
