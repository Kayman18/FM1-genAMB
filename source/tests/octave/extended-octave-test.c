/* SPDX-License-Identifier: MPL-2.0
 * The production parameter, pitch and project code, with the existing native
 * rate harness's hardware sinks. No application or firmware image is built.
 */
#define main existing_rate_test_main
#include "../rates/evolve_rate_test.c"
#undef main

static void extended_parameters(void)
{
    const int octaves[] = {P_PADOCT, P_PAD2OCT, P_BOCT, P_FMOCT};
    char text[16];
    en_init();
    for (unsigned i = 0; i < sizeof octaves / sizeof octaves[0]; ++i) {
        int p = octaves[i];
        assert(en_param_info(p)->lo == -4 && en_param_info(p)->hi == 2);
        assert(en_param_info(p)->def == 0);
        for (int v = -4; v <= 2; ++v) { apply(p, v); assert(par[p] == v); }
        apply(p, -5); assert(par[p] == -4);
        apply(p, 3); assert(par[p] == 2);
        en_param_text(p, -3, text); assert(!strcmp(text, "-3"));
        en_param_text(p, -4, text); assert(!strcmp(text, "-4"));
    }
    puts("PASS all four octave bounds, defaults, engine setters, and negative labels");
}

static void original_registers(void)
{
    unsigned cases = 0;
    for (int root = 0; root < 12; ++root) for (int mode = 0; mode < MODE_PIANO; ++mode) {
        par[P_ROOT] = root; pool = EN_MODE_POOL[mode]; scale_set();
        for (deg = 0; deg < nsteps; ++deg)
            for (int l = 0; l < EN_NLOOP; ++l) for (int off = 0; off < 28; ++off) {
                par[P_BOCT] = par[P_FMOCT] = -2;
                int r = bell_note(l, off), f = fm_original_pitch(l, off);
                assert(r == f && r >= 36 && r <= 103);
                for (int oct = -3; oct >= -4; --oct) {
                    par[P_BOCT] = par[P_FMOCT] = (int16_t)oct;
                    int rn = bell_note(l, off), fn = fm_original_pitch(l, off);
                    assert(rn == r + 12 * (oct + 2));
                    assert(fn == f + 12 * (oct + 2));
                    assert(rn >= 0 && rn <= 127 && rn % 12 == r % 12);
                    ++cases;
                }
            }
    }
    printf("PASS %u Original Rings/FM mappings lower by exactly one/two additional octaves\n", cases);
}

static void pad_registers(void)
{
    unsigned cases = 0;
    for (int octave = -4; octave <= 2; ++octave) for (int pc = 0; pc < 12; ++pc)
        for (int drift = -12; drift <= 12; drift += 12) {
            int n = en_pad_pitch(48 + pc, drift, octave);
            assert(n >= 0 && n <= 127 && n % 12 == pc);
            if (octave >= -3 || drift >= 0)
                assert(n == 48 + pc + drift + 12 * octave);
            else assert(n == pc); /* downward drift cannot underflow MIDI0 */
            ++cases;
        }
    en_init(); en_playing = 1; par[P_MIDI] = 1;
    par[P_PADRANGE] = par[P_PAD2RANGE] = 2;
    par[P_PADCHANCE] = par[P_PAD2CHANCE] = 100;
    for (int octave = -4; octave <= -3; ++octave) for (int pc = 0; pc < 12; ++pc) {
        par[P_PADOCT] = par[P_PAD2OCT] = (int16_t)octave;
        pad_t a = {0}, b = {0}; a.target = b.target = (float)(48 + pc);
        for (int i = 0; i < 128; ++i) {
            pad_breathe(&a, 0); pad2_breathe(&b, 0);
            assert(a.emitted_note < 128 && b.emitted_note < 128);
            assert(a.emitted_note % 12 == pc && b.emitted_note % 12 == pc);
            assert(a.emitted_note == en_pad_pitch(a.midi_note, a.seq_shift, octave));
            assert(b.emitted_note == en_pad_pitch(b.midi_note, b.seq_shift, octave));
        }
    }
    printf("PASS %u Pad pitch bounds and 6144 real breath events without MIDI underflow\n", cases);
}

static void evolve_registers(void)
{
    unsigned cases = 0;
    for (int root = 0; root < 12; ++root) for (int mode = 0; mode < MODE_PIANO; ++mode) {
        par[P_ROOT] = root; pool = EN_MODE_POOL[mode]; scale_set(); deg = 0;
        chord_nt = en_chord_tones(pool, root, deg, chord_pcs);
        for (int follow = 0; follow <= 1; ++follow) for (int span = 1; span <= 3; ++span)
            for (int oct = -4; oct <= -3; ++oct) for (int v = 0; v < 128; ++v) {
                par[P_BOCT] = par[P_FMOCT] = (int16_t)oct;
                par[P_EFOLLOW] = par[P_FMEFOLLOW] = (int16_t)follow;
                par[P_ERANGE] = par[P_FMERANGE] = (int16_t)span;
                int r = evo_pitch(v), f = fm_evo_pitch(v), base = 60 + root + 12 * oct;
                assert(r == f && r >= base && r < base + 12 * span);
                int ok = 0;
                if (follow) { for (int i = 0; i < chord_nt; ++i) ok |= r % 12 == chord_pcs[i]; }
                else { for (int i = 0; i < nsteps; ++i) ok |= r % 12 == (root + steps[i]) % 12; }
                assert(ok);
                ++cases;
            }
    }
    printf("PASS %u Evolve mappings at the new low octaves retain chord/scale notes\n", cases);
}

static void extended_persistence(void)
{
    const int octaves[] = {P_PADOCT, P_PAD2OCT, P_BOCT, P_FMOCT};
    for (int v = -4; v <= 2; ++v) {
        project_defaults();
        for (unsigned p = 0; p < sizeof octaves / sizeof octaves[0]; ++p) {
            proj.par[octaves[p]] = (int16_t)v;
            for (int s = 0; s < NPIECES; ++s) proj.slot[s].par[octaves[p]] = (int16_t)(s % 7 - 4);
        }
        assert(project_save() == 0); project_t before = proj;
        memset(&proj, 0, sizeof proj);
        assert(project_load() == 0 && !memcmp(&before, &proj, sizeof proj));
        project_apply(); drain();
        for (unsigned p = 0; p < sizeof octaves / sizeof octaves[0]; ++p) assert(par[octaves[p]] == v);
    }
    puts("PASS active project and all piece slots retain -4..+2 through save/load/apply");
}

static void manual_rings_bounds(void)
{
    en_init();
    for (int octave = -4; octave <= 2; ++octave) for (int key = 0; key < 128; ++key) {
        q_w = q_r = 0;
        en_rings_note(key + 12 * octave, 96);
        assert(q_w == 1);
        int transmitted = (q[0] >> 16) & 255u;
        int expected = key + 12 * octave;
        if (expected < 0) expected = 0;
        if (expected > 127) expected = 127;
        assert(transmitted == expected);
    }
    puts("PASS 896 manually played Rings pitches cannot wrap in the command queue");
}

int main(void)
{
    extended_parameters(); original_registers(); pad_registers(); evolve_registers(); extended_persistence(); manual_rings_bounds();
    puts("Extended octave source checks PASSED; firmware build remains on hold.");
    return 0;
}
