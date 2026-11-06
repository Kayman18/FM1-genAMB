static void dusk2_ui_controls_tests(void)
{
    project_t saved=proj;
    assert(VIEW_KNOB[V_RINGS][0]==P_RMODEL);
    assert(VIEW_KNOB[V_RINGS][1]==P_RBURST);
    assert(VIEW_KNOB[V_RINGS][2]==P_RPOLY);
    assert(VIEW_KNOB[V_RINGS][3]==K_NONE);
    ui.mod_route=0;ui.mod_row=1;
    assert(bound_view_knob(V_MOD,2)==K_MOD_DEPTH);
    assert(bound_view_knob(V_MOD,3)==K_NONE);
    for(int fi=0;fi<2;fi++)for(int mode=0;mode<4;mode++){
        int view=fi?V_FMSEQ:V_RINGSEQ;
        knob_set(fi?P_FMSEQMODE:P_RSEQMODE,mode);
        set_view(fi?V_FM:V_DAMBIENT);
        button(fi?B_SEL:B_SEQ);
        assert(ui.view==view && ui.seq_zone[fi]==1 && dusk_bank()==1);
        int want[4]={fi?P_FMOCT:P_BOCT,fi?P_FMELENGTH:P_ELENGTH,
                     fi?P_FPROTLEN:P_RPROTLEN,fi?P_FUDSEED:P_RUDSEED};
        assert(dusk_knob(0)==want[mode]);
        for(int bank=1;bank<=2;bank++){
            ui.seq_zone[fi]=bank;
            for(int k=0;k<4;k++){
                int id=dusk_knob(k),lo,hi;
                if(id==K_NONE || id==K_EVONEW || id==K_FMEVONEW)continue;
                knob_range(id,&lo,&hi);knob_set(id,lo);
                if(hi>lo && id!=P_RMODEL){
                    turn(EN_K1+k,id,1);
                    assert(knob_get(id)>lo);
                }
            }
        }
    }
    proj=saved;project_apply();set_view(V_GARDEN);
    puts("PASS DUSK2 controls: Rings Model/Burst/Voices/blank, LFO Depth K3; SEQ and SEL enter active banks and edit all four sequencer modes");
}
