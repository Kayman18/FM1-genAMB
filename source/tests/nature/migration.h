/* Simulator-only checks. Exercise the production loader and all eight slots. */
static void nature_migration(void)
{
    project_defaults();
    project_v22_t old = legacy_record(23);
    old.piece = 3;
    old.leds = 2;
    old.par[P_PAD] = 93;
    for (int i=0; i<NPIECES; ++i) {
        old.slot[i].par[P_RBRIGHT] = 36+i;
        old.slot[i].par[P_FUDSEED] = 711+i;
        old.slot[i].pool = EN_MODE_POOL[i];
    }
    memset(&old.evo, 0x17, sizeof old.evo);
    memset(&old.fm_evo, 0x23, sizeof old.fm_evo);
    memset(&old.proteus, 0x32, sizeof old.proteus);
    memset(&old.unitd, 0x41, sizeof old.unitd);
    old.par[P_VABLEND]=48;
    for(int i=0;i<NPIECES;i++)old.slot[i].par[P_VABLEND]=48;
    assert(plat_store_save(OBJ_PROJ,&old,sizeof old)==0);
    assert(project_load()==0 && proj.format==PROJ_FORMAT);
    assert(proj.leds==2 && proj.piece==3 && proj.pool==old.pool);
    assert(!memcmp(proj.par,old.par,sizeof old.par));
    for (int i=0; i<NPIECES; ++i) {
        assert(!memcmp(proj.slot[i].par,old.slot[i].par,sizeof old.slot[i].par));
        assert(proj.slot[i].pool==old.slot[i].pool);
    }
    assert(!memcmp(&proj.evo,&old.evo,sizeof old.evo));
    assert(!memcmp(&proj.fm_evo,&old.fm_evo,sizeof old.fm_evo));
    assert(!memcmp(&proj.proteus,&old.proteus,sizeof old.proteus));
    assert(!memcmp(&proj.unitd,&old.unitd,sizeof old.unitd));
    for (int i=-1; i<NPIECES; ++i) {
        int16_t *p=i<0?proj.par:proj.slot[i].par;
        for(int k=P_NATURE_MASTER;k<P_NPARAMS;++k) assert(p[k]==en_param_info(k)->def);
    }
    /* Encode an actual TEST3 row layout, preserving independent Nature and route values. */
    disk_project_t v24 = {0};
    v24.magic=PROJ_MAGIC; v24.format=24; v24.piece=proj.piece; v24.leds=proj.leds;
    v24.evo=proj.evo; v24.fm_evo=proj.fm_evo; v24.proteus=proj.proteus; v24.unitd=proj.unitd;
    for (int i=-1; i<NPIECES; ++i) {
        disk_row_t *row = i<0 ? &v24.active : &v24.slot[i];
        memcpy(row->par, i<0 ? proj.par : proj.slot[i].par, sizeof row->par);
        row->pool = i<0 ? proj.pool : proj.slot[i].pool;
        row->par[P_NATURE_ANIMALS]=60+i;
        row->par[P_M8DEST]=15;
    }
    assert(plat_store_save(OBJ_PROJ,&v24,sizeof v24)==0 && project_load()==0);
    for (int i=-1; i<NPIECES; ++i) {
        const int16_t *p = i<0 ? proj.par : proj.slot[i].par;
        assert(!memcmp(p, i<0 ? v24.active.par : v24.slot[i].par, sizeof v24.active.par));
        assert(p[P_VACUT]==42 && p[P_VARES]==22);
    }
    disk_project_t legacy=v24;
    legacy.format=25;
    for(int i=-1;i<NPIECES;i++){
        disk_row_t *row=i<0?&legacy.active:&legacy.slot[i];
        signed char *tail=(signed char *)&row->par[P_NATURE_MASTER];
        for(int j=0;j<5;j++)tail[j]=31+j;
        tail[5]=-45;tail[6]=25;
        row->par[P_VABLEND]=13;row->par[P_FMPOLY]=6;
        row->par[P_M9DEST]=17;
    }
    assert(plat_store_save(OBJ_PROJ,&legacy,sizeof legacy)==0 && project_load()==0);
    for(int i=-1;i<NPIECES;i++){
        int16_t *p=i<0?proj.par:proj.slot[i].par;
        assert(p[P_VABLEND]==48 && p[P_FMPOLY]==4 && p[P_M8DEST]==15 && p[P_M9DEST]==0);
        assert(p[P_VACUT]==42 && p[P_VARES]==22 && p[P_VAPRESET]==0);
        for(int j=0;j<5;j++)assert(p[P_NATURE_MASTER+j]==31+j);
    }
    disk_project_t release=v24;
    release.format=26;
    for(int i=-1;i<NPIECES;i++){
        disk_row_t *row=i<0?&release.active:&release.slot[i];
        int16_t *p=i<0?proj.par:proj.slot[i].par;
        signed char *bytes=(signed char *)row->par;
        for(int j=0;j<40;j++)bytes[j]=(signed char)p[j];
        p[P_M7DEST]=14;
        memcpy(bytes+40,p+40,(P_M12DEST-40)*sizeof(*p));
    }
    assert(plat_store_save(OBJ_PROJ,&release,sizeof release)==0 && project_load()==0);
    for(int i=-1;i<NPIECES;i++){
        int16_t *p=i<0?proj.par:proj.slot[i].par;
        assert(p[P_M7DEST]==14 && !p[P_M12DEST] && !p[P_M13DEST] && !p[P_M14DEST]);
    }
    assert(!memcmp(&v24.unitd,&proj.unitd,sizeof proj.unitd));
    for (int i=0; i<NPIECES; ++i) {
        project_piece(i);
        proj.par[P_VACUT]=(int16_t)(100-12*i);
        proj.par[P_VARES]=(int16_t)(i==7 ? 32 : 1+i);
        for (int j=0; j<5; ++j) proj.par[P_NATURE_MASTER+j]=(int16_t)(i*10+j);
    }
    assert(project_save()==0);
    memset(&proj,0,sizeof proj);
    assert(project_load()==0 && sizeof(disk_project_t)==3840);
    for (int i=0; i<NPIECES; ++i) {
        project_piece(i);
        assert(proj.par[P_VACUT]==100-12*i && proj.par[P_VARES]==(i==7 ? 32 : 1+i));
        for (int j=0; j<5; ++j) assert(proj.par[P_NATURE_MASTER+j]==i*10+j);
    }
    project_piece_reset();
    for (int j=P_NATURE_MASTER;j<P_NPARAMS;++j) assert(proj.par[j]==en_param_info(j)->def);
    proj.par[P_NATURE_WATER]=101;
    assert(write_record()==0 && project_load()!=0);
    assert(plat_store_save(OBJ_PROJ,&old,sizeof old-1)==0 && project_load()!=0);
    project_defaults(); project_apply();
    puts("PASS v23 layout, all 167 prior values, v24 migration, eight pieces, four banks, v25 and v26 migration and v27 round trip, recipe reset and invalid records");
}
