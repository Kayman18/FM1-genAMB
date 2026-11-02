/* SPDX-License-Identifier: ISC
 * Included only by the HOST simulator; exercises the production loader. */
static void classic_migration_tests(void) {
    project_t before=proj;
    const int mapped[7]={FM_FILTER_CLEAN,FM_FILTER_POLYSIX,FM_FILTER_SEM,
        FM_FILTER_SEM,FM_FILTER_JUNO,FM_FILTER_SEM,FM_FILTER_SEM};
    int count=0;
    for(int m=0;m<7;m++)for(int t=0;t<4;t++) {
        project_defaults();
        proj.par[P_FFILTERMODEL]=m;proj.par[P_FFILTERTYPE]=t;
        proj.par[P_FFILTERCUT]=37;proj.par[P_FFILTERRES]=62;
        proj.par[P_FMPOLY]=10;proj.par[P_FMPATCH]=15;
        for(int i=0;i<NPIECES;i++){
            proj.slot[i].par[P_FFILTERMODEL]=(m+i)%7;
            proj.slot[i].par[P_FFILTERTYPE]=(t+i)%4;
            proj.slot[i].par[P_M10SPEED]=i*20;
            proj.slot[i].par[P_FUDSEED]=i*10+7;
        }
        proj.format=21;
        project_t expected=proj;expected.format=22;expected.par[P_FMPOLY]=6;
        for(int i=-1;i<NPIECES;i++){
            int16_t *par=i<0?expected.par:expected.slot[i].par;
            int old=par[P_FFILTERMODEL],type=par[P_FFILTERTYPE];
            par[P_FFILTERMODEL]=mapped[old];
            if(old==0&&type==3){par[P_FFILTERMODEL]=0;type=0;}
            else if(old==2)type=1;
            else if(old>=1&&old<=5)type=0;
            par[P_FFILTERTYPE]=type;
        }
        assert(plat_store_save(OBJ_PROJ,&proj,sizeof proj)==0);
        memset(&proj,0,sizeof proj);
        assert(project_load()==0);
        assert(!memcmp(&proj,&expected,sizeof proj));
        count++;
    }
    printf("PASS %d format-21 filter translations, active project and all eight pieces; all unrelated bytes preserved\n",count);
    for(int m=0;m<7;m++) {
        project_defaults();project_v20_t old={0};
        old.magic=PROJ_MAGIC;old.format=20;old.pool=proj.pool;
        old.leds=proj.leds;old.piece=proj.piece;
        for(int j=0;j<V20_NPARAMS;j++)old.par[j]=proj.par[j];
        old.par[P_FFILTERMODEL]=m;old.par[P_FFILTERTYPE]=0;
        old.par[P_FFILTERCUT]=27;old.par[P_M11SPEED]=180;
        for(int i=0;i<NPIECES;i++){
            old.slot[i].pool=proj.slot[i].pool;
            for(int j=0;j<V20_NPARAMS;j++)old.slot[i].par[j]=old.par[j];
        }
        old.evo=proj.evo;old.fm_evo=proj.fm_evo;old.proteus=proj.proteus;
        assert(plat_store_save(OBJ_PROJ,&old,sizeof old)==0);
        assert(project_load()==0 && proj.format==22);
        assert(proj.par[P_FFILTERMODEL]==mapped[m]);
        assert(proj.par[P_FFILTERCUT]==27 && proj.par[P_M11SPEED]==180);
        for(int j=0;j<V20_NPARAMS;j++)if(j!=P_FFILTERMODEL&&j!=P_FFILTERTYPE)
            assert(proj.par[j]==old.par[j]);
        assert(!memcmp(&proj.evo,&old.evo,sizeof old.evo));
        assert(!memcmp(&proj.fm_evo,&old.fm_evo,sizeof old.fm_evo));
        assert(!memcmp(&proj.proteus,&old.proteus,sizeof old.proteus));
    }
    puts("PASS format-20 migration for all old models, other parameters and pattern banks retained");
    for(int m=0;m<6;m++) {
        project_defaults();proj.par[P_FFILTERMODEL]=m;
        proj.par[P_FFILTERTYPE]=0;proj.par[P_FMPOLY]=6;
        assert(project_save()==0);project_t expected=proj;
        memset(&proj,0,sizeof proj);assert(project_load()==0);
        assert(!memcmp(&proj,&expected,sizeof proj));
    }
    puts("PASS format-22 save/load for all six model choices");
    for(int invalid=-1;invalid<=7;invalid+=8) {
        project_defaults();proj.format=21;proj.par[P_FFILTERMODEL]=invalid;
        assert(plat_store_save(OBJ_PROJ,&proj,sizeof proj)==0);
        assert(project_load()!=0 && proj.format==22);
    }
    puts("PASS invalid legacy model IDs rejected before lookup");
    for (int voices=1; voices<=11; ++voices) {
        project_defaults();assert(proj.par[P_FMPOLY]==4);
        proj.par[P_FMPOLY]=voices;proj.par[P_FMPATCH]=13;
        for(int i=0;i<NPIECES;i++)proj.slot[i].par[P_FMPOLY]=voices;
        project_t expected=proj;
        expected.par[P_FMPOLY]=voices>6?6:voices;
        for(int i=0;i<NPIECES;i++)expected.slot[i].par[P_FMPOLY]=voices>6?6:voices;
        assert(plat_store_save(OBJ_PROJ,&proj,sizeof proj)==0);
        int result=project_load();
        if(voices<=10){assert(result==0);assert(!memcmp(&proj,&expected,sizeof proj));}
        else assert(result!=0 && proj.par[P_FMPOLY]==4);
    }
    puts("PASS four-voice default, 1..6 preserved, 7..10 capped across all slots, corrupt 11 rejected");
    proj=before;
}
