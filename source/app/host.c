/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 in fm1-base's host simulator (host/plat_host.c, which includes app.c and so this): AMB-1's checks for
 * "expect", its command, a reboot and the screen reader's dump.
 *
 * AMB-1's checks: playing frozen view piece pool dirty sixteenths ext pad_level bass_level chordroot chordN loopnotes
 * parN (fm1-base's own: store_writes clocks_out midi_out peak_db_max peak_db_min). Its command: storev3 N (the
 * project as AMB-1 0.9 before the layer pages wrote it, format 3, the playing piece's Pad at N). */

#include <assert.h>
#include "../tests/pending/wide_host.h"
void hallucigenia_tests(void);
#include "../tests/pending/layout_host.h"
#include "../tests/filter/classic_migration_host.h"


#include "../tests/dusk/ui_controls_host.h"
#include "../tests/dusk/pad_filter_controls_host.h"

static en_evo_bank_t host_evo_saved, host_fmevo_saved;
static en_proteus_bank_t host_prot_saved;
static uint32_t host_chord_stamp;
int app_expect(const char *what, int *got)
{
    int g;
    if (!strcmp(what,"chord_changed")) {
        uint32_t cur=0;
        for(int i=0;i<4;++i)cur=(cur<<8)|(uint32_t)en_chord[i];
        g=cur!=host_chord_stamp;
    }
    else if (!strcmp(what,"rseq_active")) g=en_ring_seq_active_mode;
    else if (!strcmp(what,"fmseq_active")) g=en_fm_seq_active_mode;
    else if (!strcmp(what,"rseq_zone")) g=ui.seq_zone[0];
    else if (!strcmp(what,"fmseq_zone")) g=ui.seq_zone[1];
    else if (!strcmp(what,"prot_r_pos")) g=en_prot_pos[0];
    else if (!strcmp(what,"prot_f_pos")) g=en_prot_pos[1];
    else if (!strcmp(what,"prot_pattern_same")) {
        en_proteus_bank_t cur;en_proteus_snapshot(&cur);g=1;
        for(int l=0;l<2;l++)
            if(cur.seq[l].length!=host_prot_saved.seq[l].length ||
               memcmp(cur.seq[l].note,host_prot_saved.seq[l].note,32) ||
               memcmp(cur.seq[l].restorder,host_prot_saved.seq[l].restorder,32)) g=0;
    }
    else if (!strcmp(what,"prot_same")) {en_proteus_bank_t cur;en_proteus_snapshot(&cur);g=!memcmp(&cur,&host_prot_saved,sizeof cur);
}
    else if (!strcmp(what,"prot_chord_fail")) g=(int)(prot_chord_failure[0]+prot_chord_failure[1]);
    else if (!strcmp(what,"unitd_note_r")) g=(int)ud_note_count[0];
    else if (!strcmp(what,"unitd_note_f")) g=(int)ud_note_count[1];
    else if (!strcmp(what,"unitd_chord_fail")) g=(int)(ud_chord_fail[0]+ud_chord_fail[1]);
    else if (!strcmp(what,"unitd_options_row_r")) g=(int)ui.unit_row[0];
    else if (!strcmp(what,"unitd_options_row_f")) g=(int)ui.unit_row[1];
    else if (!strcmp(what,"prot_count_r")) g=(int)prot_note_count[0];
    else if (!strcmp(what,"prot_count_f")) g=(int)prot_note_count[1];
    else if (!strcmp(what,"fm_evo_mode")) g=en_fm_evo_active;
    else if (!strcmp(what,"fm_evo_pattern")) g=en_fm_evo_pattern+1;
    else if (!strcmp(what,"fm_evo_same")) { en_evo_bank_t current;en_fmevo_snapshot(&current);g=!memcmp(&current,&host_fmevo_saved,sizeof current); }
    else if (!strcmp(what,"fm_level")) g=(int)(en_fm_level*100.0f);
    else if (!strcmp(what,"fm_manual_count")) {
        g=0;for(int i=0;i<NKEYS;++i)g+=fm_key_note[i]!=0;for(int i=0;i<128;++i)g+=fm_midi_note[i]!=0;
    } else if (!strncmp(what,"fm_key",6)) { int k=atoi(what+6);g=(k>=0&&k<NKEYS)?(int)fm_key_note[k]-1:-1; }
    else if (!strncmp(what,"fm_midi",7)) { int k=atoi(what+7);g=(k>=0&&k<128)?(int)fm_midi_note[k]-1:-1; }
    else if (!strcmp(what,"fm_loopnotes")) {
        g=0;for(int l=0;l<EN_NLOOP;l++)for(int i=0;i<EN_LOOP_MAX;i++)g+=en_fm_loop_note[l][i]>=0;
    }
    else if (!strcmp(what,"evo_mode")) g=en_evo_active;
    else if (!strcmp(what,"evo_pattern")) g=en_evo_pattern+1;
    else if (!strcmp(what,"evo_same")) { en_evo_bank_t current;en_evo_snapshot(&current);g=!memcmp(&current,&host_evo_saved,sizeof current); }
    else if (!strcmp(what,"format")) g=proj.format;
    else if (!strcmp(what, "perf_mode")) { perf_stats_t s; perf_read(&s); g=s.dual; }
    else if (!strcmp(what, "perf_seconds")) { perf_stats_t s; perf_read(&s); g=s.seconds; }
    else if (!strcmp(what, "playing"))
        g = en_playing;
    else if (!strcmp(what, "frozen"))
        g = en_frozen;
    else if (!strcmp(what, "view"))
        g = ui.view;
    else if (!strcmp(what, "piece"))
        g = proj.piece;
    else if (!strcmp(what, "pool"))               /* the scale, relative to the root (hex ok) */
        g = proj.pool;
    else if (!strcmp(what, "dirty"))
        g = ui.dirty;
    else if (!strcmp(what, "sixteenths"))
        g = (int)en_bars;
    else if (!strcmp(what, "ext"))
        g = en_ext;
    else if (!strcmp(what, "pad_level"))
        g = (int)(en_pad_level * 100.0f);
    else if (!strcmp(what, "bass_level"))
        g = (int)(en_bass_level * 100.0f);
    else if (!strcmp(what, "chordroot"))
        g = en_chord_root;
    else if (!strncmp(what, "chord", 5))          /* chordN: pad voice N's note */
        g = en_chord[atoi(what + 5) & 3];
    else if (!strcmp(what, "loopnotes")) {        /* the notes in all the loops */
        int l, i;
        g = 0;
        for (l = 0; l < EN_NLOOP; l++)
            for (i = 0; i < EN_LOOP_MAX; i++)
                g += en_loop_note[l][i] >= 0;
    } else if (!strncmp(what, "par", 3))          /* parN: parameter N (amb.h P_*) */
        g = proj.par[atoi(what + 3)];
    else
        return 0;
    *got = g;
    return 1;
}

int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    (void)b, (void)c, (void)d;
    if (!strcmp(cmd,"pad_filter_controls_tests")) {pad_filter_controls_tests();return 1;}
    if (!strcmp(cmd,"dusk2_ui_controls_tests")) {dusk2_ui_controls_tests();return 1;}
    if (!strcmp(cmd,"pad_movement_tests")) {extern void pad_movement_tests(void);pad_movement_tests();return 1;}
    if (!strcmp(cmd,"pad_roles_tests")) {extern void pad_roles_tests(void);pad_roles_tests();return 1;}
    if (!strcmp(cmd,"duskview")) { int v=atoi(a);assert(v>=0 && v<NVIEWS);set_view(v);return 1; }
    if (!strcmp(cmd,"hallucigenia_tests")) {hallucigenia_tests();return 1;}
    if (!strcmp(cmd,"wide_layout_tests")) {wide_layout_tests();return 1;}
    if (!strcmp(cmd,"pending_layout_tests")) {pending_layout_tests();return 1;}
    if (!strcmp(cmd,"classic_migration_tests")) {classic_migration_tests();return 1;}
    if (!strcmp(cmd,"monumatic_panic")) { app_panic(); return 1; }
    if (!strcmp(cmd,"setpar")) { int p=atoi(a),v=atoi(b);if(p>=0&&p<P_NPARAMS){proj.par[p]=v;en_set(p,v);mark_dirty();}return 1; }
    if (!strcmp(cmd,"chordhash")) {
        host_chord_stamp=0;
        for(int i=0;i<4;++i)host_chord_stamp=(host_chord_stamp<<8)|(uint32_t)en_chord[i];
        return 1;
    }
    if (!strcmp(cmd,"prothash")) {en_proteus_snapshot(&host_prot_saved);return 1;}
    if (!strcmp(cmd,"protnew")) {en_proteus_new(atoi(a));mark_dirty();return 1;}
    if (!strcmp(cmd,"storev20")) {
        project_v20_t old;memset(&old,0,sizeof old);
        old.magic=PROJ_MAGIC;old.format=20u;old.pool=proj.pool;
        old.leds=proj.leds;old.piece=proj.piece;
        for(int k=0;k<V20_NPARAMS;k++)old.par[k]=proj.par[k];
        for(int i=0;i<NPIECES;i++){
            old.slot[i].pool=proj.slot[i].pool;
            for(int k=0;k<V20_NPARAMS;k++)old.slot[i].par[k]=proj.slot[i].par[k];
        }
        en_evo_snapshot(&old.evo);en_fmevo_snapshot(&old.fm_evo);
        en_proteus_snapshot(&old.proteus);
        return plat_store_save(OBJ_PROJ,&old,sizeof old)==0;
    }
    if (!strcmp(cmd,"storev19")) {
        project_v19_t o;
        memset(&o,0,sizeof o);o.magic=PROJ_MAGIC;o.format=19u;
        o.pool=proj.pool;o.leds=proj.leds;o.piece=proj.piece;
        for(int k=0;k<V19_NPARAMS;k++)o.par[k]=proj.par[k];
        for(int i=0;i<NPIECES;i++){
            o.slot[i].pool=proj.slot[i].pool;
            for(int k=0;k<V19_NPARAMS;k++)o.slot[i].par[k]=proj.slot[i].par[k];
        }
        en_evo_snapshot(&o.evo);en_fmevo_snapshot(&o.fm_evo);
        en_proteus_snapshot(&o.proteus);
        return plat_store_save(OBJ_PROJ,&o,sizeof o)==0;
    }
    if (!strcmp(cmd,"storev18")) {
        project_v18_t o;
        memset(&o,0,sizeof o);o.magic=PROJ_MAGIC;o.format=18u;
        o.pool=proj.pool;o.leds=proj.leds;o.piece=proj.piece;
        for(int k=0;k<V18_NPARAMS;k++)o.par[k]=proj.par[k];
        for(int i=0;i<NPIECES;i++) {
            o.slot[i].pool=proj.slot[i].pool;
            for(int k=0;k<V18_NPARAMS;k++)o.slot[i].par[k]=proj.slot[i].par[k];
        }
        en_evo_snapshot(&o.evo);en_fmevo_snapshot(&o.fm_evo);
        plat_store_save(OBJ_PROJ,&o,sizeof o);return 1;
    }
    if (!strcmp(cmd,"evohash")) {en_evo_snapshot(&host_evo_saved);return 1;}
    if (!strcmp(cmd,"fmevohash")) {en_fmevo_snapshot(&host_fmevo_saved);return 1;}
    if (!strcmp(cmd,"fmevonew")) {en_fmevo_new();mark_dirty();return 1;}
    if (!strcmp(cmd, "storev17")) {
        project_v17_t o;
        memset(&o,0,sizeof o);o.magic=PROJ_MAGIC;o.format=17u;
        o.pool=proj.pool;o.leds=proj.leds;o.piece=proj.piece;
        for(int k=0;k<V17_NPARAMS;k++)o.par[k]=proj.par[k];
        o.par[P_PAD]=(int16_t)atoi(a);
        for(int i=0;i<NPIECES;i++) {
            o.slot[i].pool=proj.slot[i].pool;
            for(int k=0;k<V17_NPARAMS;k++)o.slot[i].par[k]=proj.slot[i].par[k];
        }
        en_evo_snapshot(&o.evo);
        plat_store_save(OBJ_PROJ,&o,sizeof o);return 1;
    }
    if (!strcmp(cmd, "storev16")) {
        project_v16_t o;
        memset(&o,0,sizeof o); o.magic=PROJ_MAGIC; o.format=16u;
        o.pool=proj.pool; o.leds=proj.leds; o.piece=proj.piece;
        for(int k=0;k<V16_NPARAMS;k++) o.par[k]=proj.par[k];
        o.par[P_PAD]=(int16_t)atoi(a);
        for(int i=0;i<NPIECES;i++) {
            o.slot[i].pool=proj.slot[i].pool;
            for(int k=0;k<V16_NPARAMS;k++) o.slot[i].par[k]=proj.slot[i].par[k];
        }
        plat_store_save(OBJ_PROJ,&o,sizeof o); return 1;
    }
    if (!strcmp(cmd, "storev15")) {
        project_v15_t o;
        memset(&o,0,sizeof o); o.magic=PROJ_MAGIC; o.format=15u;
        o.pool=proj.pool; o.leds=proj.leds; o.piece=proj.piece;
        for(int k=0;k<V15_NPARAMS;k++) o.par[k]=proj.par[k];
        o.par[P_PAD]=(int16_t)atoi(a);
        for(int i=0;i<NPIECES;i++) {
            o.slot[i].pool=proj.slot[i].pool;
            for(int k=0;k<V15_NPARAMS;k++) o.slot[i].par[k]=proj.slot[i].par[k];
        }
        plat_store_save(OBJ_PROJ,&o,sizeof o); return 1;
    }
    if (!strcmp(cmd, "storev3")) {
        static project_v3_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 3u;
        for (k = 0; k < V3_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_PAD] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V3_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        o.slot[proj.piece].par[P_PAD] = o.par[P_PAD];
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    return 0;
}

void app_host_reboot(void) { memset(&proj, 0, sizeof proj); }

void app_speak_dump(void) { speak_dump(); }
