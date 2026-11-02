/* SPDX-License-Identifier: GPL-3.0-only
 * Meaningful adapter regressions: cached block continuity, real envelope/gate
 * behavior, polyphony/stealing, patch retention, exact factory decoding, and
 * all 32 core algorithms with legal parameter extremes under sanitizers.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EN_HOST 1
#include "../../app/fm_port/fm_port.c"
#define ZN_PATCH_DATA
#include "../../app/vendor/zen_fm/zen_patches.h"

static unsigned passed, failed;
#define CHECK(label, condition) do { \
    if (condition) { ++passed; printf("PASS %s\n", label); } \
    else { ++failed; printf("FAIL %s (line %d)\n", label, __LINE__); } \
} while (0)

static float left[32768], right[32768], sendbuf[32768], reference[32768];
static double render_peak;

static double render_samples(unsigned n, unsigned stride)
{
    unsigned at = 0, i;
    double energy = 0.0;
    render_peak = 0.0;
    while (at < n) {
        unsigned count = n - at;
        if (count > stride)
            count = stride;
        memset(left, 0, count * sizeof(float));
        memset(right, 0, count * sizeof(float));
        memset(sendbuf, 0, count * sizeof(float));
        fm_port_render_add(left, right, sendbuf, 0.37f, count);
        for (i = 0; i < count; ++i) {
            if (!isfinite(left[i]) || fabsf(left[i]) >= 4.0f || left[i] != right[i] ||
                sendbuf[i] != left[i] * 0.37f) {
                CHECK("finite bounded stereo audio and exact reverb send", 0);
                return -1.0;
            }
            if (fabs((double)left[i]) > render_peak)
                render_peak = fabs((double)left[i]);
            energy += (double)left[i] * left[i];
        }
        at += count;
    }
    return energy;
}

static unsigned held_count(void)
{
    unsigned i, n = 0;
    for (i = 0; i < FM_PORT_VOICES; ++i)
        n += state.voice[i].core.live && state.voice[i].core.down;
    return n;
}

static int note_live(int note)
{
    unsigned i;
    for (i = 0; i < FM_PORT_VOICES; ++i)
        if (state.voice[i].core.live && state.voice[i].note == note)
            return 1;
    return 0;
}

static void factory_and_memory(void)
{
    unsigned i, j;
    uint32_t retained_hash = 2166136261u, existing_hash = 2166136261u;
    int okay = 1;
    uint8_t expanded[156], packed[128];
    CHECK("adapter, expanded and packed factory counts agree at 16",
          FM_PORT_PATCHES == 16 && FM_FACTORY_COUNT == FM_PORT_PATCHES && ZN_NPATCH == FM_PORT_PATCHES);
    for (i = 0; i < FM_PORT_PATCHES; ++i) {
        size_t name_len = strlen(fm_port_patch_name((int)i));
        fm6_unpack(ZN_PATCH[i], expanded);
        fm6_pack(FM_FACTORY[i], packed);
        if (memcmp(expanded, FM_FACTORY[i], 156) || memcmp(packed, ZN_PATCH[i], 128))
            okay = 0;
        while (name_len && fm_port_patch_name((int)i)[name_len - 1] == ' ')
            --name_len;
        if (name_len > 9u)
            okay = 0;
    }
    CHECK("all 16 factory voices decode/repack exactly and names fit the knob display", okay);
    for (i = 3; i < 9; ++i)
        for (j = 0; j < 128; ++j) {
            retained_hash ^= ZN_PATCH[i][j];
            retained_hash *= 16777619u;
        }
    CHECK("Drop through Furin retain their exact upstream data and order",
          retained_hash == 0x979927bdu);
    for (i = 0; i < 9; ++i)
        for (j = 0; j < 128; ++j) {
            existing_hash ^= ZN_PATCH[i][j];
            existing_hash *= 16777619u;
        }
    CHECK("all nine existing genAMB patches keep their exact data and indices",
          existing_hash == 0x4dd43141u);
    CHECK("six voices and sixteen LFOs occupy the accounted 2256 bytes",
          FM_PORT_VOICES == 6 && sizeof(fm_voice_t) == 312u && sizeof state == 2256u);
    CHECK("voice + LFO pool allocation stays below ten-voice 4000-byte budget",
          sizeof state <= 4000u);
    printf("Memory: note=%zu voice=%zu pool=%zu shared buffers=%zu bytes\n",
           sizeof(fm6_note_t), sizeof(fm_voice_t), sizeof state,
           sizeof(fm6_bus) + sizeof(cached) + sizeof(temporary));
    printf("Bank: %u patches, factory flash=%zu LFO state=%zu automatic LFO arrays=%zu bytes\n",
           FM_PORT_PATCHES, sizeof FM_FACTORY, sizeof state.lfo,
           2u * FM_PORT_PATCHES * sizeof(int32_t));
    fm_port_init();
    CHECK("FM adapter defaults to four voices", polyphony == 4u);
    fm_port_set_polyphony(FM_PORT_VOICES);
    fm_port_set_patch(15);
    fm_port_trigger(60, 100, 0);
    CHECK("slot 16 is selectable and its voice retains the last preset",
          state.voice[0].patch == 15 && strncmp(fm_port_patch_name(15), "Reso Bass", 9) == 0);
    fm_port_set_patch(999);
    fm_port_trigger(64, 100, 0);
    fm_port_set_patch(-1);
    fm_port_trigger(67, 100, 0);
    CHECK("expanded bank bounds clamp safely without changing existing voice presets",
          state.voice[0].patch == 15 && state.voice[1].patch == 15 && state.voice[2].patch == 0);
}

static void sample_multiply(void)
{
    /* The device must avoid a 64-bit product mixed with loads, but replacing
     * that product must preserve every low-level FM sample. Exercise all sine
     * inputs at gain boundaries and one million independently generated pairs.
     */
    static const int32_t gains[] = {
        -16, -1, 0, 1, 16, 1119, 1120, 32767, 32768, 65535,
        (1 << 24) - 1, 1 << 24, (1 << 27) - 1, 1 << 27, (1 << 27) + 16
    };
    unsigned i;
    int32_t sine;
    uint32_t random = 0x46714d31u;
    int same = 1;
    for (i = 0; i < sizeof gains / sizeof gains[0]; ++i)
        for (sine = -32768; sine <= 32767; ++sine)
            if (fm6_mul_q15(sine, gains[i]) !=
                (int32_t)(((int64_t)sine * gains[i]) >> 15))
                same = 0;
    for (i = 0; i < 1000000; ++i) {
        int32_t amplitude;
        random = random * 1664525u + 1013904223u;
        sine = (int32_t)(random >> 16) - 32768;
        random = random * 1664525u + 1013904223u;
        amplitude = (int32_t)(random & 0x7ffffffu);
        if (fm6_mul_q15(sine, amplitude) !=
            (int32_t)(((int64_t)sine * amplitude) >> 15))
            same = 0;
    }
    CHECK("32-bit sample multiply is exact against signed 64-bit reference", same);
}

static void envelopes_and_events(void)
{
    unsigned p;
    int audible = 1, all_done = 1;
    fm_port_init();
    CHECK("FM adapter defaults to four voices", polyphony == 4u);
    fm_port_set_polyphony(FM_PORT_VOICES);
    fm_port_set_patch(4);
    fm_port_trigger(60, 110, 0);
    CHECK("FM starts muted", render_samples(4096, 256) == 0.0);
    for (p = 0; p < FM_PORT_PATCHES; ++p) {
        double energy;
        fm_port_init();
    CHECK("FM adapter defaults to four voices", polyphony == 4u);
    fm_port_set_polyphony(FM_PORT_VOICES);
        fm_port_set_level(100);
        fm_port_set_patch((int)p);
        fm_port_trigger(60, 110, 88200);
        energy = render_samples(132300, 257);
        printf("  patch %u %-10s energy %.9f\n", p, fm_port_patch_name((int)p), energy);
        if (energy < 0.00001)
            audible = 0;
        fm_port_all_notes_off();
        render_samples(44100u * 20u, 256);
        if (fm_port_active_voices)
            all_done = 0;
    }
    CHECK("all 16 real patches produce audible finite audio", audible);
    CHECK("all patches release completely after note-off", all_done);

    fm_port_init();
    CHECK("FM adapter defaults to four voices", polyphony == 4u);
    fm_port_set_polyphony(FM_PORT_VOICES);
    fm_port_set_level(100);
    fm_port_set_patch(4);
    render_samples(1, 1);          /* cache 31 silent samples */
    fm_port_trigger(60, 110, 0);
    CHECK("strike preserves the 31 previously computed samples", render_samples(31, 31) == 0.0);
    CHECK("strike sounds at next block (not delayed an extra block)", render_samples(32, 32) > 0.0);
    fm_port_note_off(60);
    CHECK("manual note-off releases keyed note", held_count() == 0u);

    fm_port_init();
    CHECK("FM adapter defaults to four voices", polyphony == 4u);
    fm_port_set_polyphony(FM_PORT_VOICES);
    fm_port_set_patch(0);
    fm_port_trigger(60, 110, 33);
    render_samples(32, 32);
    CHECK("33-sample gate remains held after first block", held_count() == 1u);
    render_samples(32, 32);
    CHECK("gate rounds upward to next block boundary", held_count() == 1u);
    render_samples(1, 1);
    CHECK("timed gate releases on the correct next block", held_count() == 0u);

    fm_port_init();
    CHECK("FM adapter defaults to four voices", polyphony == 4u);
    fm_port_set_polyphony(FM_PORT_VOICES);
    fm_port_set_patch(6); /* Rin is long enough to test an explicitly held gate. */
    fm_port_trigger(60, 100, 0);
    render_samples(44100u * 3u, 256);
    CHECK("zero gate sustains until explicit release", held_count() == 1u);
    fm_port_all_notes_off();
    CHECK("gentle stop releases every held note", held_count() == 0u);

    fm_port_init();
    CHECK("FM adapter defaults to four voices", polyphony == 4u);
    fm_port_set_polyphony(FM_PORT_VOICES);
    fm_port_set_level(100);
    fm_port_set_patch(4);
    fm_port_trigger(60, 100, 0);
    render_samples(129, 129);
    fm_port_panic();
    CHECK("panic explicitly clears voices and pending sound", render_samples(129, 129) == 0.0 &&
          fm_port_active_voices == 0u);
}

static void allocation(void)
{
    unsigned i;
    fm_port_init();
    CHECK("FM adapter defaults to four voices", polyphony == 4u);
    fm_port_set_polyphony(FM_PORT_VOICES);
    fm_port_set_patch(0);
    for (i = 0; i < FM_PORT_VOICES; ++i)
        fm_port_trigger(60 + (int)i, 100, 0);
    CHECK("six simultaneous notes allocate six independent voices", held_count() == 6u);
    fm_port_note_off(62);
    fm_port_trigger(70, 100, 0);
    CHECK("stealing prefers a released voice over held notes", !note_live(62) && note_live(60) &&
          note_live(61) && note_live(63) && note_live(64) && note_live(65) && note_live(70));
    fm_port_trigger(71, 100, 0);
    CHECK("all-held stealing replaces the oldest voice", !note_live(60) && note_live(71));
    fm_port_trigger(71, 90, 3000);
    CHECK("retrigger of the same held pitch reuses its voice", held_count() == 6u);
    fm_port_set_polyphony(2);
    CHECK("lower polyphony releases excess held notes without discarding their tails",
          held_count() == 2u && state.voice[5].core.live && !state.voice[5].core.down);
    fm_port_trigger(90, 127, 0);
    CHECK("new allocation respects reduced polyphony", held_count() == 2u && note_live(90));

    {
        int voices, okay = 1;
        for (voices = 1; voices <= FM_PORT_VOICES; ++voices) {
            fm_port_panic();
            fm_port_set_polyphony(voices);
            for (i = 0; i < (unsigned)voices + 2u; ++i)
                fm_port_trigger(48 + (int)i, 100, 0);
            if (held_count() != (unsigned)voices ||
                !note_live(48 + voices + 1) || note_live(48))
                okay = 0;
        }
        for (voices = FM_PORT_VOICES - 1; voices >= 1; --voices) {
            fm_port_set_polyphony(voices);
            fm_port_trigger(90 + voices, 100, 0);
            if (held_count() > (unsigned)voices || !note_live(90 + voices))
                okay = 0;
        }
        CHECK("all polyphony settings 1..6 allocate and steal within their selected limit", okay);
        fm_port_set_polyphony(-10);
        CHECK("polyphony below range clamps to one", polyphony == 1u);
        fm_port_set_polyphony(100);
        for (i = 0; i < FM_PORT_VOICES; ++i)
            fm_port_trigger(72 + (int)i, 100, 0);
        CHECK("polyphony above range clamps to six and restores all six slots",
              polyphony == 6u && held_count() == 6u);
    }

    fm_port_init();
    CHECK("FM adapter defaults to four voices", polyphony == 4u);
    fm_port_set_polyphony(FM_PORT_VOICES);
    fm_port_set_patch(14); /* Held bass envelopes keep gate observations unambiguous. */
    for (i = 0; i < 4; ++i)
        fm_port_trigger(60 + (int)i, 100, 0);
    fm_port_trigger(64, 100, 33);
    fm_port_trigger(65, 100, 65);
    render_samples(64, 17);
    CHECK("fifth and sixth voices retain their independent timed gates", held_count() == 6u);
    render_samples(32, 7);
    CHECK("fifth voice gate releases while sixth and four manual voices remain held",
          held_count() == 5u && !state.voice[4].core.down && state.voice[5].core.down);
    render_samples(1, 1);
    CHECK("sixth voice gate releases at its own rounded boundary",
          held_count() == 4u && !state.voice[5].core.down);
    fm_port_all_notes_off();
    CHECK("all-notes-off releases all six slots", held_count() == 0u);

    fm_port_init();
    CHECK("FM adapter defaults to four voices", polyphony == 4u);
    fm_port_set_polyphony(FM_PORT_VOICES);
    fm_port_trigger(60, 100, 0);
    fm_port_set_patch(5);
    fm_port_trigger(64, 100, 0);
    CHECK("preset changes retain sounding voices on their original patch",
          state.voice[0].patch == 0 && state.voice[1].patch == 5 && held_count() == 2u);
    serial = 0xFFFFFFFEu;
    fm_port_panic();
    for (i = 0; i < FM_PORT_VOICES; ++i)
        fm_port_trigger(60 + (int)i, 100, 0);
    fm_port_trigger(80, 100, 0);
    CHECK("voice age remains correct across 32-bit counter wrap", !note_live(60) && note_live(80));
}

static void six_voice_audio(void)
{
    static const int notes[6] = {48, 55, 60, 64, 67, 72};
    unsigned p, k;
    int audible = 1, all_done = 1;
    double peak = 0.0;
    for (p = 0; p < FM_PORT_PATCHES; ++p) {
        double energy;
        fm_port_init();
    CHECK("FM adapter defaults to four voices", polyphony == 4u);
    fm_port_set_polyphony(FM_PORT_VOICES);
        fm_port_set_level(100);
        fm_port_set_patch((int)p);
        for (k = 0; k < 6u; ++k)
            fm_port_trigger(notes[k], 127, 27563);
        energy = render_samples(88200, 257);
        if (energy <= 0.00001)
            audible = 0;
        if (render_peak > peak)
            peak = render_peak;
        printf("  six-note patch %u %-10s peak %.6f energy %.6f\n",
               p, fm_port_patch_name((int)p), render_peak, energy);
        fm_port_all_notes_off();
        render_samples(44100u * 20u, 256);
        if (fm_port_active_voices)
            all_done = 0;
    }
    CHECK("all 16 patches produce finite audible six-note audio", audible);
    CHECK("all six-note chords retire completely after release", all_done);
    printf("Six-note maximum raw adapter peak: %.6f (before app mix/master)\n", peak);
}

static void timeline_event(unsigned step)
{
    switch (step) {
    case 0: fm_port_set_patch(4); fm_port_trigger(60, 100, 5000); break;
    case 1:
        for (int i = 0; i < 5; ++i)
            fm_port_trigger(64 + i * 2, 83 + i * 5, 8000 + (uint32_t)i * 32u);
        break;
    case 2: fm_port_set_patch(5); fm_port_trigger(67, 119, 0); break;
    case 3: fm_port_set_polyphony(2); break;
    case 4: fm_port_note_off(60); break;
    case 5: fm_port_set_level(67); break;
    case 6: fm_port_set_tune(12); break;
    case 7: fm_port_all_notes_off(); break;
    case 8: fm_port_set_patch(3); fm_port_trigger(72, 127, 7000); break;
    }
}

static void timeline(float *output, int irregular)
{
    static const unsigned time[] = {0, 157, 678, 1031, 2417, 4051, 9031, 14017, 22000, 32768};
    static const unsigned chunk[] = {1, 17, 33, 256, 7, 511, 5};
    unsigned at = 0, event = 0, index = 0;
    fm_port_init();
    CHECK("FM adapter defaults to four voices", polyphony == 4u);
    fm_port_set_polyphony(FM_PORT_VOICES);
    fm_port_set_level(100);
    memset(output, 0, 32768 * sizeof(float));
    memset(right, 0, sizeof right);
    memset(sendbuf, 0, sizeof sendbuf);
    while (at < 32768u) {
        unsigned n;
        if (at == time[event])
            timeline_event(event++);
        n = time[event] - at;
        if (irregular && n > chunk[index % 7u])
            n = chunk[index % 7u];
        ++index;
        fm_port_render_add(output + at, right + at, sendbuf + at, 0.37f, n);
        at += n;
    }
}

static void block_continuity(void)
{
    timeline(reference, 0);
    timeline(left, 1);
    CHECK("arbitrary subblocks preserve bit-identical audio across events and controls",
          memcmp(reference, left, sizeof reference) == 0);
}

static void init_core_voice(uint8_t *p)
{
    unsigned k, j;
    memset(p, 0, 156);
    for (k = 0; k < 6; ++k) {
        uint8_t *op = p + 21 * k;
        for (j = 0; j < 4; ++j) {
            op[FP_R1 + j] = 99;
            op[FP_L1 + j] = j == 3 ? 0 : 99;
        }
        op[FP_BP] = 39;
        op[FP_FC] = 1;
        op[FP_DET] = 7;
        op[FP_OL] = 99;
    }
    for (j = 0; j < 4; ++j) {
        p[FP_PR1 + j] = 99;
        p[FP_PL1 + j] = 50;
    }
    p[FP_TRNSP] = 24;
    p[FP_LFS] = 99;
    p[FP_LKS] = 1;
    memcpy(p + FP_NAME, "TEST      ", 10);
}

static void all_algorithms(void)
{
    uint8_t p[156];
    fm6_note_t voice;
    fm6_lfo_t lfo;
    int32_t out[FM6_N];
    unsigned alg, variant, block, j;
    double energy = 0.0;
    uint32_t random = 1;
    for (alg = 0; alg < 32; ++alg) {
        for (variant = 0; variant < 6; ++variant) {
            int midi = variant < 2 ? 0 : variant < 4 ? 60 : 127;
            init_core_voice(p);
            p[FP_ALG] = (uint8_t)alg;
            p[FP_FB] = variant & 1u ? 7 : 0;
            p[FP_LAMD] = p[FP_LPMD] = 99;
            p[FP_LPMS] = 7;
            p[FP_LFW] = (uint8_t)variant;
            for (j = 0; j < 6; ++j) {
                p[j * FP_OP + FP_AMS] = 3;
                p[j * FP_OP + FP_KVS] = 7;
                p[j * FP_OP + FP_LD] = p[j * FP_OP + FP_RD] = 99;
                p[j * FP_OP + FP_LC] = p[j * FP_OP + FP_RC] = variant & 3u;
            }
            memset(&voice, 0, sizeof voice);
            memset(&lfo, 0, sizeof lfo);
            fm6_lfo_reset(&lfo, p);
            fm6_lfo_key(&lfo);
            fm6_note_init(&voice, p, midi, variant & 1u ? 127 : 1, 1);
            for (block = 0; block < 128; ++block) {
                if (block == 64)
                    fm6_note_key(&voice, p, 0);
                if (fm6_note_compute(&voice, p, out, fm6_lfo_sample(&lfo), fm6_lfo_delay(&lfo),
                                    fm6_logfreq_cents((float)midi * 100.0f), alg, p[FP_FB], DT0, 0))
                    for (j = 0; j < FM6_N; ++j)
                        energy += fabs((double)out[j]);
            }
        }
    }
    CHECK("all 32 algorithms render at low/high pitch, feedback and modulation extremes",
          energy > 0.0 && isfinite(energy));
    /* Real packed-voice import bounds matter for a future SysEx loader; exercise
     * fixed oscillators, extreme scaling and both signs of pitch-envelope bends.
     */
    for (variant = 0; variant < 128; ++variant) {
        uint8_t packed[128];
        for (j = 0; j < 128; ++j) {
            random = random * 1664525u + 1013904223u;
            packed[j] = (uint8_t)(random >> 24) & 127u;
        }
        fm6_unpack(packed, p);
        memset(&voice, 0, sizeof voice);
        memset(&lfo, 0, sizeof lfo);
        fm6_lfo_reset(&lfo, p);
        fm6_lfo_key(&lfo);
        fm6_note_init(&voice, p, (int)variant, 127, 1);
        for (block = 0; block < 32; ++block)
            fm6_note_compute(&voice, p, out, fm6_lfo_sample(&lfo), fm6_lfo_delay(&lfo),
                             fm6_logfreq_cents((float)variant * 100.0f), p[FP_ALG], p[FP_FB], DT0, 0);
    }
    CHECK("128 arbitrary sanitized packed voices render without invalid arithmetic", 1);
}

int main(void)
{
    factory_and_memory();
    sample_multiply();
    envelopes_and_events();
    allocation();
    six_voice_audio();
    block_continuity();
    all_algorithms();
    printf("FM port: %u passed, %u failed\n", passed, failed);
    return failed ? 1 : 0;
}
