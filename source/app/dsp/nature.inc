/* SPDX-License-Identifier: MPL-2.0
 * Recorded Nature layer. Included in amb.c so both native builds and the
 * standalone audio regression runners use the same implementation.
 */
#include "nature.h"
#include "animal_wave_layout.h"
#include <string.h>
typedef struct { const uint8_t *data; uint32_t frames; } NatureAsset;
#include "nature_bank.inc"

enum { BUILTIN_CALLS = sizeof nature_bank / sizeof nature_bank[0] - 2,
       MAX_CALLS = BUILTIN_CALLS + ANIMAL_WAVE_COUNT };
static NatureAsset wave_calls[ANIMAL_WAVE_COUNT];
static unsigned animal_count = BUILTIN_CALLS;

static uint32_t nature_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t nature_crc32(const uint8_t *p, unsigned size)
{
    uint32_t crc = 0xffffffffu;
    while (size--) {
        crc ^= *p++;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

/* Boot only, before the audio thread starts. The ten-call bank becomes
 * available after both its directory and all encoded data are verified. */
void en_nature_mount(const uint8_t *p, unsigned capacity)
{
    animal_count = BUILTIN_CALLS;
    if (!p || capacity < ANIMAL_WAVE_LENGTH || memcmp(p, "GNANIM02", 8) ||
        nature_u32(p+8) != 1 || nature_u32(p+12) != ANIMAL_WAVE_COUNT ||
        nature_u32(p+16) != ANIMAL_WAVE_LENGTH ||
        nature_u32(p+60) != ANIMAL_WAVE_HEAD_CRC ||
        nature_crc32(p,60) != ANIMAL_WAVE_HEAD_CRC ||
        nature_u32(p+20) != ANIMAL_WAVE_DATA_CRC ||
        nature_crc32(p+ANIMAL_WAVE_HEADER,ANIMAL_WAVE_LENGTH-ANIMAL_WAVE_HEADER) != ANIMAL_WAVE_DATA_CRC)
        return;
    uint32_t next = ANIMAL_WAVE_HEADER;
    for (unsigned i = 0; i < ANIMAL_WAVE_COUNT; ++i) {
        const uint8_t *entry = p + 24 + 12*i;
        uint32_t offset = nature_u32(entry), frames = nature_u32(entry+4), bytes = nature_u32(entry+8);
        if (offset != next || bytes > ANIMAL_WAVE_LENGTH-next || !frames ||
            frames > (ANIMAL_WAVE_LENGTH/516u)*1024u || bytes != ((frames+1023u)/1024u)*516u)
            return;
        wave_calls[i].data = p+offset;
        wave_calls[i].frames = frames;
        next += bytes;
    }
    if (next == ANIMAL_WAVE_LENGTH) animal_count = MAX_CALLS;
}

unsigned en_nature_calls(void) { return animal_count; }

static const NatureAsset *nature_animal_asset(unsigned id)
{
    return id < BUILTIN_CALLS ? &nature_bank[2+id] : &wave_calls[id-BUILTIN_CALLS];
}

typedef struct {
    const NatureAsset *asset;
    uint32_t pos, phase, remaining;
    int predictor, step_index;
    float previous, next, level, left, right;
    float pan_end, pan_step, left_step, right_step;
    unsigned pan_left;
} NatureVoice;
typedef struct { float value, target, step; unsigned remaining; } NatureGain;
typedef struct {
    NatureGain gain[4];
    NatureVoice water[2], rain[2], bird[2];
    uint32_t weather_rng, bird_rng, phase_pos, phase_length, bird_wait, reply_wait;
    float weather_step;
    uint8_t phase, density, scheduling, bag[MAX_CALLS], bag_index, last_bird;
} NatureState;

static NatureState nature EN_POOL;
volatile uint32_t nature_status;

static uint32_t nature_random(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

static uint32_t nature_between(uint32_t *state, uint32_t low, uint32_t high)
{
    return low + nature_random(state) % (high - low + 1);
}

static void nature_reset(uint32_t weather_seed, uint32_t bird_seed)
{
    memset(&nature, 0, sizeof nature);
    nature.weather_rng = weather_seed;
    nature.bird_rng = bird_seed;
    nature.bag_index = (uint8_t)animal_count;
    nature.last_bird = 255;
    nature_status = 0;
}

/* One RAM lookup decodes both the signed sample delta and the next IMA
 * step. The largest delta needs 17 signed bits. All 1,424 entries fit in
 * 5,696 bytes of the working RAM released by the Rings delay. */
static int32_t nature_ima_table[89][16];
static void nature_init_decoder(void)
{
    static const int16_t steps[89] = {
        7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,
        60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,
        337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,
        1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,
        4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,
        15289,16818,18500,20350,22385,24623,27086,29794,32767
    };
    static const int8_t index_delta[8] = {-1,-1,-1,-1,2,4,6,8};
    for (int index = 0; index < 89; ++index) {
        int step = steps[index];
        for (int code = 0; code < 16; ++code) {
            int amount = (step >> 3) + ((code & 1) ? step >> 2 : 0)
                       + ((code & 2) ? step >> 1 : 0) + ((code & 4) ? step : 0);
            int next = index + index_delta[code & 7];
            next = next < 0 ? 0 : next > 88 ? 88 : next;
            nature_ima_table[index][code] = ((code & 8) ? -amount : amount) * 128 + next;
        }
    }
}

static int nature_decode(NatureVoice *v, int loop)
{
    if (v->pos == v->asset->frames) {
        if (!loop) return 0;
        v->pos = 0;
    }
    uint32_t pos = v->pos++;
    unsigned in_block = pos & 1023u;
    const uint8_t *block = v->asset->data + (pos >> 10) * 516u;
    if (!in_block) {
        v->predictor = (int16_t)(block[0] | ((uint16_t)block[1] << 8));
        v->step_index = block[2] > 88 ? 88 : block[2];
    } else {
        unsigned i = in_block - 1u;
        unsigned code = (block[4 + i / 2] >> ((i & 1) * 4)) & 15u;
        int32_t entry = nature_ima_table[v->step_index][code];
        int p = v->predictor + (entry >> 7);
        v->predictor = p < -32768 ? -32768 : p > 32767 ? 32767 : p;
        v->step_index = entry & 127;
    }
    return v->predictor;
}

static void nature_start(NatureVoice *v, const NatureAsset *asset, unsigned offset)
{
    v->asset = asset;
    v->pos = offset;
    v->phase = 0;
    v->remaining = (v->asset->frames * 441u + 159u) / 160u;
    v->previous = (float)nature_decode(v, 0) / 32768.0f;
    v->next = (float)nature_decode(v, 0) / 32768.0f;
}

/* Launch the right channel at the ADPCM block immediately preceding the
 * midpoint. Block headers carry predictor and step state for independent seek. */
static void nature_loop(NatureVoice voices[2], unsigned index)
{
    nature_start(&voices[0], &nature_bank[index], 0);
    nature_start(&voices[1], &nature_bank[index], (nature_bank[index].frames / 2u) & ~1023u);
}

static inline float nature_sample(NatureVoice *v, int loop)
{
    float out = v->previous + (v->next - v->previous) * ((float)v->phase * (1.0f / 44100.0f));
    v->phase += 16000u;
    if (v->phase >= 44100u) {
        v->phase -= 44100u;
        v->previous = v->next;
        v->next = (float)nature_decode(v, loop) / 32768.0f;
    }
    if (!loop && !--v->remaining) v->asset = 0;
    return out;
}

static void nature_weather(unsigned phase)
{
    static const uint16_t seconds[][2] = {{0,0},{45,90},{90,180},{60,120},{90,240}};
    nature.phase = (uint8_t)phase;
    nature.phase_pos = 0;
    nature.phase_length = nature_between(&nature.weather_rng, 44100u * seconds[phase][0],
                                        44100u * seconds[phase][1]);
    nature.weather_step = 1.0f / (float)(nature.phase_length - 1u);
}

static uint32_t nature_gap(void)
{
    unsigned d = nature.density - 1u;
    unsigned low_ms = 20000u - 18000u * d / 99u;
    unsigned high_ms = 60000u - 54000u * d / 99u;
    return nature_between(&nature.bird_rng, (low_ms * 441u + 9u) / 10u,
                          (high_ms * 441u + 9u) / 10u);
}

static void nature_control(int slot, int value)
{
    if (slot == 4) {
        if (nature.density != value) {
            nature.density = (uint8_t)value;
            nature.reply_wait = 0;
            if (nature.scheduling && value) nature.bird_wait = nature_gap();
        }
        return;
    }
    NatureGain *g = &nature.gain[slot];
    float target = value * 0.01f;
    if (g->target == target) return;
    g->target = target;
    if (slot != 0 && !nature.gain[0].value && !nature.gain[0].target) {
        g->value = target;
        g->remaining = 0;
        return;
    }
    g->remaining = 2205;
    g->step = (target - g->value) * (1.0f / 2205.0f);
}

static void nature_start_bird(void)
{
    NatureVoice *v = !nature.bird[0].asset ? &nature.bird[0] : &nature.bird[1];
    if (v->asset) return;
    if (nature.bag_index >= animal_count) {
        for (unsigned i = 0; i < animal_count; ++i) nature.bag[i] = (uint8_t)i;
        for (unsigned i = animal_count - 1; i; --i) {
            unsigned j = nature_between(&nature.bird_rng, 0, i);
            uint8_t tmp = nature.bag[i];
            nature.bag[i] = nature.bag[j];
            nature.bag[j] = tmp;
        }
        if (nature.bag[0] == nature.last_bird) {
            nature.bag[0] = nature.bag[1];
            nature.bag[1] = nature.last_bird;
        }
        nature.bag_index = 0;
    }
    nature.last_bird = nature.bag[nature.bag_index++];
    nature_start(v, nature_animal_asset(nature.last_bird), 0);
    v->level = (float)nature_between(&nature.bird_rng, 72, 100) * 0.01f;
    unsigned placement = nature_between(&nature.bird_rng, 0, 39);
    float pan = (0.15f + (float)(placement % 20) * 0.01f);
    if (placement >= 20) pan = 1.0f - pan;
    v->pan_end = 1.0f - pan;
    v->pan_step = (v->pan_end - pan) / (float)v->remaining;
    v->pan_left = 0;
    v->left = fm_sqrtf(1.0f - pan);
    v->right = fm_sqrtf(pan);
}

/* Equal-power positions are calculated every 64 output frames. Only two
 * gain additions are needed per sample in between. Each voice has its own
 * trajectory from the chosen side to the opposite side over its full call. */
static inline void nature_advance_pan(NatureVoice *v)
{
    if (!v->pan_left) {
        unsigned n = v->remaining < 64u ? v->remaining : 64u;
        float pan = v->pan_end - v->pan_step * (float)(v->remaining - n);
        float inv = n == 64u ? 1.0f/64.0f : 1.0f/(float)n;
        v->left_step = (fm_sqrtf(1.0f-pan) - v->left) * inv;
        v->right_step = (fm_sqrtf(pan) - v->right) * inv;
        v->pan_left = n;
    }
    v->left += v->left_step;
    v->right += v->right_step;
    --v->pan_left;
}

static inline void nature_birds(void)
{
    if (nature.reply_wait && !--nature.reply_wait) nature_start_bird();
    if (!--nature.bird_wait) {
        if (nature.bird[0].asset && nature.bird[1].asset) {
            nature.bird_wait = 4410u;
            return;
        }
        nature_start_bird();
        nature.bird_wait = nature_gap();
        unsigned chance = 20u + 20u * (nature.density - 1u) / 99u;
        if (nature_random(&nature.bird_rng) % 100u < chance)
            nature.reply_wait = nature_between(&nature.bird_rng, 17640u, 61740u);
    }
}

static inline void nature_smooth(NatureGain *g)
{
    if (g->remaining) {
        if (--g->remaining) g->value += g->step;
        else g->value = g->target;
    }
}

static void nature_render(float *left, float *right, unsigned n)
{
    /* A muted master incurs only this block-level check. */
    if (!nature.gain[0].target && !nature.gain[0].value && !nature.gain[0].remaining) {
        memset(left, 0, n * sizeof *left);
        memset(right, 0, n * sizeof *right);
        nature_status = 0;
        return;
    }
    /* genAMB: nature_smooth() is a no-op for a gain whose ramp has finished,
     * and only nature_control() at control rate can start one, so if none of
     * the four is travelling at the top of the block none can start inside it
     * and all four calls do nothing for every sample. Test once instead of
     * four loads and four branches per sample. */
    const int ramping = nature.gain[0].remaining || nature.gain[1].remaining
                     || nature.gain[2].remaining || nature.gain[3].remaining;
    for (unsigned i = 0; i < n; ++i) {
        if (ramping)
            for (unsigned g = 0; g < 4; ++g) nature_smooth(&nature.gain[g]);
        const float master = nature.gain[0].value;
        float wl = 0.0f, wr = 0.0f, rl = 0.0f, rr = 0.0f, bl = 0.0f, br = 0.0f;
        if (master && nature.gain[1].value) {
            if (!nature.water[0].asset) nature_loop(nature.water, 0);
            float level = master * nature.gain[1].value;
            wl = level * nature_sample(&nature.water[0], 1);
            wr = level * nature_sample(&nature.water[1], 1);
        } else nature.water[0].asset = 0;
        if (master && nature.gain[2].value) {
            if (!nature.rain[0].asset) {
                nature_loop(nature.rain, 1);
                nature_weather(NATURE_FADE_IN);
            }
            float env = nature.phase == NATURE_WET ? 1.0f : 0.0f;
            if (nature.phase == NATURE_FADE_IN || nature.phase == NATURE_FADE_OUT) {
                float t = fm_minf(1.0f, (float)nature.phase_pos * nature.weather_step);
                float fade = (t * t) * (3.0f - 2.0f * t);
                env = nature.phase == NATURE_FADE_IN ? fade : 1.0f - fade;
            }
            float level = (master * nature.gain[2].value) * env;
            if (level > 0.0f) {
                rl = level * nature_sample(&nature.rain[0], 1);
                rr = level * nature_sample(&nature.rain[1], 1);
            }
            if (++nature.phase_pos == nature.phase_length)
                nature_weather(nature.phase == NATURE_DRY ? NATURE_FADE_IN : nature.phase + 1u);
        } else {
            nature.rain[0].asset = 0;
            nature.phase = NATURE_RAIN_OFF;
        }
        if (master && nature.gain[3].value) {
            int can_schedule = nature.density && nature.gain[0].target && nature.gain[3].target;
            if (can_schedule && !nature.scheduling) {
                nature.bird_wait = nature_between(&nature.bird_rng, 88200u, 264600u);
                nature.scheduling = 1;
            }
            if (can_schedule) nature_birds();
            else { nature.scheduling = 0; nature.reply_wait = 0; }
            for (unsigned b = 0; b < 2; ++b) {
                NatureVoice *v = &nature.bird[b];
                if (v->asset) {
                    nature_advance_pan(v);
                    float call = ((master * nature.gain[3].value) * v->level) * nature_sample(v, 0);
                    bl += call * v->left;
                    br += call * v->right;
                }
            }
        } else {
            nature.scheduling = 0;
            nature.reply_wait = 0;
            nature.bird[0].asset = nature.bird[1].asset = 0;
        }
        left[i] = (wl + rl) + bl;
        right[i] = (wr + rr) + br;
    }
    unsigned count = (nature.bird[0].asset != 0) + (nature.bird[1].asset != 0);
    nature_status = (nature.water[0].asset != 0) | ((unsigned)nature.phase << 1) | (count << 4);
}
