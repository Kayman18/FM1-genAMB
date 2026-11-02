/* Host-only checks of production bindings, edits and saved rate IDs. */
static void pending_layout_tests(void) {
    project_t saved=proj;
    plat_host_exact=1;
    project_defaults();
    int params[]={P_ERATE,P_FMERATE,P_RPROTRATE,P_FPROTRATE,P_RUDRATE,P_FUDRATE};
    for(int p=0;p<6;p++){
        knob_set(params[p],-1);
        for(int i=0;i<13;i++){
            assert(proj.par[params[p]]==seq_rate_from_index(i));
            char label[20];en_param_text(params[p],proj.par[params[p]],label);
            assert(!strcmp(label,seq_rate_label(seq_rate_from_index(i))));
            turn(0,params[p],1);
        }
        assert(proj.par[params[p]]==3);
        for(int i=12;i>=0;i--){assert(proj.par[params[p]]==seq_rate_from_index(i));turn(0,params[p],-1);}
        assert(proj.par[params[p]]==-1);
    }
    for(int r=6;r<11;r++){
        ui.mod_route=r;ui.mod_row=1;
        assert(bound_view_knob(V_MOD,0)==K_MOD_ROUTE);
        assert(bound_view_knob(V_MOD,1)==K_MOD_DEST);
        assert(bound_view_knob(V_MOD,2)==K_NONE);
        assert(bound_view_knob(V_MOD,3)==K_MOD_DEPTH);
        int depth=knob_get(K_MOD_DEPTH);turn(0,bound_view_knob(V_MOD,3),1);assert(knob_get(K_MOD_DEPTH)==depth+1);
        ui.mod_row=2;
        assert(bound_view_knob(V_MOD,0)==K_MOD_SHAPE);
        assert(bound_view_knob(V_MOD,1)==K_NONE);
        assert(bound_view_knob(V_MOD,2)==K_MOD_SPEED);
        assert(bound_view_knob(V_MOD,3)==K_NONE);
        knob_set(K_MOD_SHAPE,0);turn(0,bound_view_knob(V_MOD,0),1);assert(knob_get(K_MOD_SHAPE)==1);
        int speed=knob_get(K_MOD_SPEED);turn(0,bound_view_knob(V_MOD,2),1);assert(knob_get(K_MOD_SPEED)==speed+1);
    }
    for(int row=1;row<=2;row++)for(int i=0;i<4;i++){
        ui.grid_row[V_SPACE]=row;int k=VIEW_KNOB[V_SPACE][i],v=proj.par[k];
        assert(bound_view_knob(V_SPACE,i)==k);turn(0,bound_view_knob(V_SPACE,i),1);assert(proj.par[k]==v+1);
    }
    for(int id=-1;id<=11;id++) {
        project_defaults();
        for(int p=0;p<6;p++) {
            proj.par[params[p]]=id;
            for(int i=0;i<NPIECES;i++)proj.slot[i].par[params[p]]=id;
        }
        project_t expected=proj;
        assert(plat_store_save(OBJ_PROJ,&proj,sizeof proj)==0);
        memset(&proj,0,sizeof proj);assert(project_load()==0);
        assert(!memcmp(&proj,&expected,sizeof proj));
    }
    plat_host_exact=0;
    proj=saved;ui.mod_route=0;ui.mod_row=1;ui.grid_row[V_SPACE]=1;
    puts("PASS all six rate controls: ordered stepping, bounds, labels and all 13 saved IDs across all pieces; M7-M11 and Reverb knob bindings");
}
