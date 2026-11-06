static void pad_filter_controls_tests(void)
{
    project_t saved=proj;project_defaults();
    assert(sizeof(project_t)==sizeof(project_v22_t));
    assert(VIEW_KNOB[V_MIXER][0]==P_PAD && VIEW_KNOB[V_MIXER][1]==P_PULSE);
    assert(VIEW_KNOB[V_MIXER][2]==P_BELLS && VIEW_KNOB[V_MIXER][3]==P_FMLEVEL);
    for(int layer=0;layer<2;layer++){
        int btn=layer?B_ARP:B_ENV,overview=layer?V_DRONE:V_PAD,body=layer?V_DRONEVOICE:V_PADBODY;
        int filter=layer?V_PAD2FILTER:V_FMFILTER;
        set_view(overview);button(btn);assert(ui.view==body);button(btn);assert(ui.view==filter);
        assert(!fm_view(filter) && ui.fm_filter_row==1);
        int oct=layer?P_PAD2OCT:P_PADOCT,old_oct=proj.par[oct];button(B_OCTUP);assert(proj.par[oct]==old_oct+1);
        ui.fm_filter_row=1;
        assert(dusk_knob(1)==(layer?P_P2FILTERCUT:P_FFILTERCUT));
        assert(dusk_knob(2)==(layer?P_P2FILTERRES:P_FFILTERRES));
        ui.fm_filter_row=2;assert(dusk_knob(0)==(layer?P_P2FILTERMODEL:P_FFILTERMODEL));
        knob_set(dusk_knob(0),FM_FILTER_SEM);
        button(btn);assert(ui.view==(layer?V_PAD2SEQ:V_PADSEQ));
    }
    set_view(V_FM);button(B_SEL);assert(ui.view==V_FMSEQ && ui.seq_zone[1]==1);
    project_defaults();project_v22_t old={0};
    old.magic=PROJ_MAGIC;old.format=22;old.pool=proj.pool;old.leds=proj.leds;old.piece=proj.piece;
    memcpy(old.par,proj.par,sizeof old.par);old.par[P_FFILTERMODEL]=0;
    old.par[P_FFILTERCUT]=53;old.par[P_FFILTERRES]=37;old.par[P_BASS]=45;old.par[P_BASSOCT]=-1;
    for(int i=0;i<NPIECES;i++){
        old.slot[i].pool=proj.slot[i].pool;memcpy(old.slot[i].par,old.par,sizeof old.par);
        old.slot[i].par[P_FFILTERCUT]=(int16_t)(50+i);
    }
    old.evo=proj.evo;old.fm_evo=proj.fm_evo;old.proteus=proj.proteus;old.unitd=proj.unitd;
    old.evo.step[0][0]=67;old.fm_evo.step[0][1]=72;
    assert(plat_store_save(OBJ_PROJ,&old,sizeof old)==0);
    memset(&proj,0,sizeof proj);assert(project_load()==0 && proj.format==23);
    const int reused[5]={P_P2FILTERMODEL,P_P2FILTERTYPE,P_P2FILTERCUT,P_P2FILTERRES,P_P2FILTERMIX};
    for(int id=0;id<P_NPARAMS;id++){
        int skip=id==P_FFILTERMODEL;for(int j=0;j<5;j++)skip|=id==reused[j];
        if(!skip){assert(proj.par[id]==old.par[id]);for(int i=0;i<NPIECES;i++)assert(proj.slot[i].par[id]==old.slot[i].par[id]);}
    }
    assert(proj.par[P_FFILTERMODEL]==FM_FILTER_CLEAN && proj.par[P_FFILTERCUT]==53 && proj.par[P_FFILTERRES]==37);
    assert(proj.par[P_P2FILTERMODEL]==FM_FILTER_CLEAN && proj.par[P_P2FILTERTYPE]==0);
    assert(proj.par[P_P2FILTERCUT]==78 && proj.par[P_P2FILTERRES]==18 && proj.par[P_P2FILTERMIX]==100);
    for(int i=0;i<NPIECES;i++)assert(proj.slot[i].par[P_FFILTERCUT]==50+i && proj.slot[i].par[P_P2FILTERMODEL]==FM_FILTER_CLEAN);
    assert(!memcmp(&proj.evo,&old.evo,sizeof old.evo) && !memcmp(&proj.unitd,&old.unitd,sizeof old.unitd));
    assert(project_save()==0);project_t expected=proj;memset(&proj,0,sizeof proj);assert(project_load()==0);
    assert(!memcmp(&expected,&proj,sizeof proj));
    proj=saved;project_apply();set_view(V_GARDEN);
    puts("PASS stereo pad filter controls, moved page navigation, mixer Pad1/Pad2/Rings/FM, format22 migration and format23 save roundtrip; unchanged save size");
}
