/* SPDX-License-Identifier: MPL-2.0
 * Cross-boundary checks for the pending parameter and saved-project changes.
 * Uses production engine/project code with the native rate harness's sinks.
 * This is not an application or firmware build.
 */
#define main existing_rate_test_main
#include "../rates/evolve_rate_test.c"
#undef main

static void retained_models_and_legacy_projects(void)
{
    static const int mapped[7] = {0, 1, 2, 2, 1, 2, 2};
    static const char *const names[3] = {"Modal", "Sympat.", "String"};
    char label[16];
    en_init();
    assert(RINGS_MODEL_COUNT == 3);
    /* Old IDs remain valid serialized data; every runtime path normalizes. */
    assert(en_param_info(P_RMODEL)->lo == 0 && en_param_info(P_RMODEL)->hi == 6);
    for (int id = 0; id <= 6; ++id) {
        en_param_text(P_RMODEL, id, label);
        assert(!strcmp(label, names[mapped[id]]));
        apply(P_RMODEL, id);
        assert(par[P_RMODEL] == mapped[id]);

        project_defaults();
        proj.par[P_RMODEL] = (int16_t)id;
        proj.par[P_RFX] = 5; /* obsolete field still accepts older valid saves */
        for (int s = 0; s < NPIECES; ++s) {
            proj.slot[s].par[P_RMODEL] = (int16_t)((s + id) % 7);
            proj.slot[s].par[P_RFX] = (int16_t)(s % 6);
        }
        project_t expected = proj;
        expected.par[P_RMODEL] = (int16_t)mapped[id];
        for (int s = 0; s < NPIECES; ++s)
            expected.slot[s].par[P_RMODEL] = (int16_t)mapped[(s + id) % 7];
        assert(plat_store_save(OBJ_PROJ, &proj, sizeof proj) == 0);
        memset(&proj, 0, sizeof proj);
        assert(project_load() == 0 && !memcmp(&proj, &expected, sizeof proj));
        project_apply(); drain();
        assert(par[P_RMODEL] == mapped[id]);
    }
    puts("PASS all seven historical model IDs: labels, runtime apply, active project and all eight pieces");

    /* Format17 migration must use the same fallback without losing its bank. */
    project_defaults();
    project_v17_t old = {0};
    old.magic = PROJ_MAGIC; old.format = 17; old.pool = proj.pool;
    for (int p = 0; p < V17_NPARAMS; ++p) old.par[p] = proj.par[p];
    old.par[P_RMODEL] = 3;
    for (int s = 0; s < NPIECES; ++s) {
        old.slot[s].pool = proj.slot[s].pool;
        for (int p = 0; p < V17_NPARAMS; ++p) old.slot[s].par[p] = proj.slot[s].par[p];
        old.slot[s].par[P_RMODEL] = (int16_t)(s % 7);
    }
    en_evo_snapshot(&old.evo);
    assert(plat_store_save(OBJ_PROJ, &old, sizeof old) == 0);
    assert(project_load() == 0 && proj.format == PROJ_FORMAT);
    assert(proj.par[P_RMODEL] == 2 && proj.par[P_FMPOLY] == 4);
    assert(!memcmp(&proj.evo, &old.evo, sizeof old.evo));
    for (int s = 0; s < NPIECES; ++s) assert(proj.slot[s].par[P_RMODEL] == mapped[s % 7]);
    puts("PASS format17 migration preserves its Evolve bank and maps every stored model");
}

static void reject_corrupt_models_before_mapping(void)
{
    for (int invalid = -1; invalid <= 7; invalid += 8) {
        for (int where = -1; where < NPIECES; ++where) {
            project_defaults(); project_t defaults = proj;
            if (where < 0) proj.par[P_RMODEL] = (int16_t)invalid;
            else proj.slot[where].par[P_RMODEL] = (int16_t)invalid;
            assert(plat_store_save(OBJ_PROJ, &proj, sizeof proj) == 0);
            assert(project_load() == -1);
            assert(!memcmp(&proj, &defaults, sizeof proj));
        }
    }
    puts("PASS invalid model IDs -1/7 rejected in active project and every piece before fallback mapping");
}

static void six_voice_parameter_and_persistence(void)
{
    en_init(); project_defaults();
    assert(FM_PORT_VOICES == 6 && FM_MIDI_VOICES == 6);
    assert(en_param_info(P_FMPOLY)->lo == 1 && en_param_info(P_FMPOLY)->hi == 6);
    assert(en_param_info(P_FMPOLY)->def == 4 && proj.par[P_FMPOLY] == 4);
    for (int s = 0; s < NPIECES; ++s) assert(proj.slot[s].par[P_FMPOLY] == 4);
    for (int voices = 1; voices <= 6; ++voices) {
        project_defaults();
        proj.par[P_FMPOLY] = (int16_t)voices;
        for (int s = 0; s < NPIECES; ++s) proj.slot[s].par[P_FMPOLY] = (int16_t)(s % 6 + 1);
        assert(project_save() == 0); project_t expected = proj;
        memset(&proj, 0, sizeof proj);
        assert(project_load() == 0 && !memcmp(&proj, &expected, sizeof proj));
        project_apply(); drain(); assert(par[P_FMPOLY] == voices);
    }
    apply(P_FMPOLY, 7); assert(par[P_FMPOLY] == 6);
    apply(P_FMPOLY, 0); assert(par[P_FMPOLY] == 1);
    project_defaults(); proj.par[P_FMPOLY] = 7;
    assert(plat_store_save(OBJ_PROJ, &proj, sizeof proj) == 0);
    assert(project_load() == -1 && proj.par[P_FMPOLY] == 4);
    puts("PASS FM 1..6 persists/applies, default four retained, runtime clamps and corrupt saved seven rejected");
}

int main(void)
{
    retained_models_and_legacy_projects();
    reject_corrupt_models_before_mapping();
    six_voice_parameter_and_persistence();
    assert(APP_CANVAS_PX == 240u * 132u);
    assert(PROJ_FORMAT == 18u);
    printf("PASS canvas %u pixels / %u bytes; project format %u, size %zu bytes\n",
           APP_CANVAS_PX, APP_CANVAS_PX * 2u, PROJ_FORMAT, sizeof(project_t));
    puts("Pending-source integration checks PASSED; firmware build remains on hold.");
    return 0;
}
