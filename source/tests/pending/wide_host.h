/* Production UI binding checks, host only. */
static void wide_layout_tests(void){
    project_t saved=proj;plat_host_exact=1;
    int views[]={V_GARDEN,V_ROOTDIV,V_SETUP,V_PAD,V_PADBODY,V_PADSEQ,V_DRONE,V_DRONEVOICE,V_PAD2SEQ,
        V_SPACE,V_REVSEND,V_REVMIX,V_RINGS,V_RINGTONE,V_RINGEXC,V_RINGDLY,V_DAMBIENT,V_RUNITOPT};
    for(unsigned v=0;v<sizeof views/sizeof views[0];v++)for(int row=1;row<=2;row++){
        int view=views[v];ui.grid_row[view]=row;ui.unit_row[0]=row;
        for(int i=0;i<4;i++){
            int k=VIEW_KNOB[view][i];assert(bound_view_knob(view,i)==k);
            if(k==K_NONE)continue;
            int lo,hi;knob_range(k,&lo,&hi);knob_set(k,lo);
            int want=hi>lo?lo+1:lo;
            if(k==P_RMODEL)want=rings_model_from_index(1);
            if(k==P_RUDRATE)want=seq_rate_from_index(1);
            turn(0,bound_view_knob(view,i),1);assert(knob_get(k)==want);
        }
    }
    for(int route=0;route<6;route++){
        ui.mod_route=route;ui.mod_row=1;
        assert(bound_view_knob(V_MOD,0)==K_MOD_ROUTE);
        assert(bound_view_knob(V_MOD,1)==K_MOD_DEST);
        assert(bound_view_knob(V_MOD,2)==K_NONE);
        assert(bound_view_knob(V_MOD,3)==K_MOD_DEPTH);
        int lo,hi;knob_range(K_MOD_DEST,&lo,&hi);assert(lo==0 && hi==0);
        knob_set(K_MOD_DEPTH,20);turn(0,bound_view_knob(V_MOD,3),1);assert(knob_get(K_MOD_DEPTH)==21);
        ui.mod_row=2;assert(bound_view_knob(V_MOD,2)==K_MOD_SPEED);
        knob_range(K_MOD_SHAPE,&lo,&hi);assert(lo==0 && hi==0);
        knob_set(K_MOD_SPEED,20);turn(0,bound_view_knob(V_MOD,2),1);assert(knob_get(K_MOD_SPEED)==21);
    }
    assert(VIEW_KNOB[V_ROOTDIV][0]==P_ROOT);
    assert(VIEW_KNOB[V_ROOTDIV][1]==P_FMDENSITY);
    assert(VIEW_KNOB[V_ROOTDIV][2]==P_DENSITY);
    for(int v=0;v<NVIEWS;v++)if(v!=V_ROOTDIV)for(int row=1;row<=2;row++)for(int slot=0;slot<4;slot++) {
        int k=grid_binding(v,row,slot);
        assert(k!=P_FMDENSITY && k!=P_DENSITY);
    }
    proj=saved;ui.mod_route=0;ui.mod_row=1;ui.unit_row[0]=1;
    for(int i=0;i<NVIEWS;i++)ui.grid_row[i]=1;
    plat_host_exact=0;
    proj.par[P_RSEQMODE]=3;en_set(P_RSEQMODE,3);
    puts("PASS 18 wide pages: all controls match knob labels in either prior row state; M1-M6 fixed routing/shape and depth/speed edits");
}
