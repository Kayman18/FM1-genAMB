#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "rings_port.cc"
#include "rings/dsp/fm_voice.h"
#include "rings/dsp/string_synth_part.h"
#include "stmlib/utils/random.h"

static void setup(int model, int poly, int fx)
{
    stmlib::Random::Seed(0x71a92b46u);
    rings_port_init();
    rings_port_set_model(model);
    rings_port_set_polyphony(poly);
    rings_port_set_string_fx(fx);
    rings_port_set_level(70);
    rings_port_set_structure(43);
    rings_port_set_brightness(59);
    rings_port_set_damping(64);
    rings_port_set_position(37);
    rings_port_set_exciter(55);
    rings_port_set_delay(57, 32, 44, 4);
    rings_port_set_ambient(50, 55, 32);
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        std::printf("Part=%zu FMVoice=%zu Follower=%zu NaiveSvf=%zu StringSynthPart=%zu Reverb=%zu\n",
            sizeof(rings::Part), sizeof(rings::FMVoice), sizeof(rings::Follower), sizeof(stmlib::NaiveSvf), sizeof(rings::StringSynthPart), sizeof(rings::Reverb));
        return 0;
    }
#ifdef CURRENT
    if (!std::strcmp(argv[1], "checks")) {
        static const int saved[] = {0, 1, 2, 2, 1, 2, 2};
        assert(RINGS_MODEL_COUNT == 3);
        for (int i=0; i<7; ++i) {
            assert(rings_model_index(i) == saved[i]);
            assert(rings_model_normalize(i) == saved[i]);
            setup(i, 4, 0);
            assert(g_model == saved[i] && g_effective_model == saved[i] && g_requested_model == saved[i]);
            assert(g_part.model() == saved[i]);
            g_part.set_model((rings::ResonatorModel)i);
            assert(g_part.model() == saved[i]);
        }
        for (int i=0; i<3; ++i) assert(rings_model_from_index(i) == i);
        assert(rings_model_normalize(INT_MIN) == 0);
        assert(rings_model_normalize(INT_MAX) == 2);
        assert(rings_model_from_index(INT_MIN) == 0);
        assert(rings_model_from_index(INT_MAX) == 2);
        rings_port_set_model(-100);
        assert(g_model == 0 && g_part.model() == rings::RESONATOR_MODEL_MODAL);
        rings_port_set_model(100);
        assert(g_model == 2 && g_part.model() == rings::RESONATOR_MODEL_STRING);
        setup(0, 4, 0);
        rings_port_set_modulation(0.30f, RP_DEST_MODEL, 100, 0, 0, 0, 0, 0, 0);
        assert(g_model_offset_latched == 1 && g_requested_model == 1 && g_effective_model == 0);
        rings_port_trigger(60, 100);
        assert(g_effective_model == 1 && g_part.model() == 1);
        assert(g_model_fade_left == 1323 && g_model_fade_total == 1323);
        float l[24]={0}, r[24]={0}, rev[24]={0};
        rings_port_render_add(l, r, rev, 0.4f, 24);
        assert(g_model_fade_left == 1299);
        rings_port_set_modulation(1.0f, RP_DEST_MODEL, 100, 0, 0, 0, 0, 0, 0);
        assert(g_model_offset_latched == 2 && g_requested_model == 2 && g_effective_model == 1);
        rings_port_trigger(67, 110);
        assert(g_effective_model == 2 && g_part.model() == 2 && g_model_fade_left == 1323);
        rings_port_set_modulation(0.72f, RP_DEST_MODEL, 100, 0, 0, 0, 0, 0, 0);
        assert(g_model_offset_latched == 2 && g_requested_model == 2);
        rings_port_set_modulation(0.70f, RP_DEST_MODEL, 100, 0, 0, 0, 0, 0, 0);
        assert(g_model_offset_latched == 1 && g_requested_model == 1 && g_effective_model == 2);
        rings_port_trigger(55, 80);
        assert(g_effective_model == 1 && g_part.model() == 1);
        rings_port_set_modulation(1, RP_DEST_MODEL, 100, 1, RP_DEST_MODEL, 100, 1, RP_DEST_MODEL, 100);
        assert(g_requested_model == 2);
        rings_port_set_modulation(-1, RP_DEST_MODEL, 100, -1, RP_DEST_MODEL, 100, -1, RP_DEST_MODEL, 100);
        assert(g_requested_model == 0);
        std::puts("PASS C++ three-model mapping, all legacy IDs, clamping, full three-model modulation span, deferred commit, hysteresis, and 1323-sample fade");
        return 0;
    }
#endif
    if (!std::strcmp(argv[1], "cloud")) {
        cloud_reverb_port_init();
        cloud_reverb_port_set(0.7f, 0.4f, 0.6f, 0.5f);
        double energy=0;
        for(int block=0; block<1000; ++block) {
            float l[192]={0}, r[192]={0};
            if(block%100==0) l[0]=0.25f;
            cloud_reverb_port_process(l,r,192);
            for(int i=0;i<192;i++) {
                assert(std::isfinite(l[i]) && std::isfinite(r[i]));
                energy+=l[i]*l[i]+r[i]*r[i];
            }
            std::fwrite(l,sizeof l,1,stdout);
            std::fwrite(r,sizeof r,1,stdout);
        }
        assert(energy>1.0e-8);
        return 0;
    }
    int model = std::atoi(argv[1]), poly = std::atoi(argv[2]), fx = std::atoi(argv[3]);
    setup(model, poly, fx);
    double energy = 0;
    for (int block=0; block<500; ++block) {
        float l[192]={0}, r[192]={0}, rev[192]={0};
        if (block%25 == 0) rings_port_trigger(40+(block/25*7)%43, 65+(block/25*9)%60);
        rings_port_render_add(l, r, rev, 0.4f, 192);
        for (int i=0; i<192; ++i) {
            assert(std::isfinite(l[i]) && std::isfinite(r[i]) && std::isfinite(rev[i]));
            energy += l[i]*l[i]+r[i]*r[i];
        }
        std::fwrite(l, sizeof l, 1, stdout);
        std::fwrite(r, sizeof r, 1, stdout);
        std::fwrite(rev, sizeof rev, 1, stdout);
    }
    assert(energy>1.0e-8);
    return 0;
}
