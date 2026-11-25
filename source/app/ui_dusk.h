/* SPDX-License-Identifier: MPL-2.0
 * Dusk Type test UI: native 240x240, two 120-row DMA bands. No extra canvas.
 * Parameter indicators are static; render only on state changes.
 */

#define D_BG RGB(21,17,25)
#define D_TEXT RGB(230,232,238)
#define D_LABEL RGB(195,174,219)
#define D_ACC RGB(231,171,144)
#define D_DIM RGB(142,147,160)
#define D_RULE RGB(48,45,58)

static void dusk_fit(int x,int y,int width,const felucca_font_t *f,const char *s,uint16_t c)
{
    char b[64];unsigned n=0;
    while(s[n] && n<sizeof b-1){b[n]=s[n];n++;}b[n]=0;
    while(n && text_w(f,b)>width){b[--n]=0;}
    cv_text(x,y,f,b,c);
}
static int dusk_bank(void)
{
    int v=ui.view;
    if(v==V_RINGSEQ || v==V_FMSEQ)return ui.seq_zone[v==V_FMSEQ]==2?2:1;
    if((v==V_FMFILTER || v==V_PAD2FILTER))return ui.fm_filter_row==2?2:1;
    if(v==V_MOD && ui.mod_route<11)return ui.mod_row==2?2:1;
    if(grid_page(v) && !wide_grid_page(v))return grid_row(v);
    return 1;
}
static int dusk_knob(int slot)
{
    if(ui.view==V_RINGSEQ || ui.view==V_FMSEQ)
        return seq_binding(ui.view,dusk_bank(),slot);
    return bound_view_knob(ui.view,slot);
}
static const char *dusk_label(int slot,int k)
{
    int v=ui.view;
    if(v==V_RINGSEQ || v==V_FMSEQ)return seq_binding_label(v,dusk_bank(),slot);
    if(v==V_MOD)return knob_name(k);
    if((v==V_FMFILTER || v==V_PAD2FILTER))return dusk_bank()==2?(slot==0?"Model":""):KNOB_LABEL[v][slot];
    if(grid_page(v) && !wide_grid_page(v))return grid_label(v,dusk_bank(),slot);
    return KNOB_LABEL[v][slot];
}
static void dusk_cell(int slot,int y,int k,const char *label,int compact)
{
    int x=12+(slot%2)*118,lo,hi,v,p=0;
    char l[48],value[64],kn[3]={'K',(char)('1'+slot),0};
    const felucca_font_t *vf=&FONT_M;
    int hot=plat_ms()<ui.touch_until && ui.touched==slot;
    cv_text(x,y,&FONT_T,kn,hot?D_ACC:D_LABEL);
    if(k==K_NONE){cv_text(x,y+20,&FONT_S,"--",D_RULE);return;}
    caps(l,label,sizeof l);
    dusk_fit(x+17,y,89,&FONT_T,l,D_LABEL);
    knob_text(k,value);
    if(text_w(vf,value)>105)vf=&FONT_S;
    if(compact)vf=&FONT_XS;
    if(text_w(vf,value)>105)vf=&FONT_XS;
    if(text_w(vf,value)>105)vf=&FONT_T;
    dusk_fit(x,y+(compact?12:20),105,vf,value,D_TEXT);
    knob_range(k,&lo,&hi);v=knob_get(k);
    if(hi>lo){p=(v-lo)*10/(hi-lo);if(p<0)p=0;if(p>10)p=10;}
    if(!compact)for(int j=0;j<10;j++)cv_rect(x+j*9,y+49,6,2,j<p?D_ACC:D_RULE);
}
static void dusk_perf(void)
{
    perf_stats_t s;char b[20];perf_read(&s);
    const char *names[]={"AUDIO AVG %","AUDIO PEAK %","CPU0 US","CPU0 PEAK US","CPU1 WORK US","WAIT US","DROPOUTS","FAULTS"};
    uint32_t vals[]={s.audio_pct,s.peak_pct,s.cpu0_us,s.cpu0_peak_us,s.worker_us,s.wait_us,s.dropouts,s.faults};
    for(int j=0;j<8;j++){
        int x=12+(j%2)*118,y=69+(j/2)*21;
        cv_text(x,y,&FONT_T,names[j],D_LABEL);
        itoa_u(vals[j],b);cv_text(x,y+10,&FONT_XS,b,(j>=6 && vals[j])?D_ACC:D_TEXT);
    }
    /* All three footer readings describe the single slowest callback since reset. */
    const char *peak_labels[] = {"PK C0", "PK C1", "PK WAIT"};
    uint32_t active = s.worst_total_us >= s.worst_wait_us ? s.worst_total_us - s.worst_wait_us : 0;
    uint32_t peak_values[] = {active, s.worst_work_us, s.worst_wait_us};
    for (int j=0; j<3; ++j) {
        int x=12 + 74*j;
        cv_text(x,224,&FONT_T,peak_labels[j],D_LABEL);
        itoa_u(peak_values[j],b);
        cv_text(x+68-text_w(&FONT_T,b),224,&FONT_T,b,D_TEXT);
    }
    for(int j=0;j<4;j++)dusk_cell(j,160+(j/2)*28,dusk_knob(j),dusk_label(j,dusk_knob(j)),1);
}
/* The saved knobs stay editable at their base values. Only this 16-pixel
 * band follows an audible FM assignment, at a maximum of eight updates/s. */
static int dusk_fm_destination(void)
{
    if (ui.view!=V_MOD || ui.help || ui.mod_route<6 || ui.mod_route>8) return 0;
    int dest=knob_get(K_MOD_DEST);
    return dest>=DEST_FM_VOLUME && dest<=DEST_VA_R ? dest : 0;
}

static int dusk_fm_live(int dest)
{
    float v=va_live[dest-DEST_FM_VOLUME];
    return (int)(v+(v<0?-0.5f:0.5f));
}
static void dusk_fm_readout(int dest,int value)
{
    char label[12],number[16];
    static const int ids[]={P_FMLEVEL,P_VACUT,P_VARES,P_VAENV,P_VATRACK,P_VAPWM,P_VAVCFA,P_VAVCFD,P_VAVCFS,P_VAVCFR};
    en_param_text(P_M7DEST,dest,label);
    en_param_text(ids[dest-DEST_FM_VOLUME],value,number);
    cv_text(12,67,&FONT_T,label,D_LABEL);
    cv_text(228-text_w(&FONT_T,number),67,&FONT_T,number,D_ACC);
}

static void dusk_paint(int live_dest, int live_value)
{
    char title[48],b[24],root[48];const felucca_font_t *tf=&FONT_L;
    int bp=plat_battery_pct();
    cv_text(12,10,&FONT_B,"genAMB",D_LABEL);
    if(bp<0){b[0]='-';b[1]='-';b[2]=0;}
    else if(bp>100){b[0]='U';b[1]='S';b[2]='B';b[3]=0;}
    else{char *p=b;itoa_u((uint32_t)bp,p);while(*p)p++;*p++='%';*p=0;}
    cv_text(228-text_w(&FONT_T,b),13,&FONT_T,b,D_DIM);
    caps(title,VIEW_NAME[ui.view],sizeof title);
    if(text_w(tf,title)>216)tf=&FONT_M;
    if(text_w(tf,title)>216)tf=&FONT_B;
    if(text_w(tf,title)>216)tf=&FONT_XS;
    dusk_fit(12,34,216,tf,title,D_TEXT);
    if (ui.view == V_NATURE) {
        unsigned density = (unsigned)proj.par[P_NATURE_DENSITY];
        char *end = b;
        itoa_u(density, b);
        while (*end) ++end;
        *end++ = '%'; *end = 0;
        cv_text(12,67,&FONT_T,"SELECT: ANIMAL DENSITY",D_LABEL);
        cv_text(228-text_w(&FONT_XS,b),65,&FONT_XS,b,D_ACC);
        cv_line(12,82,227,82,D_ACC);
    } else if(ui.view!=V_PERF){
        if(ui.view==V_RINGSEQ || ui.view==V_FMSEQ){
            static const char *const modes[]={"Original","Hallucigenia","Proteus","Unit-D"};
            int mode=proj.par[ui.view==V_FMSEQ?P_FMSEQMODE:P_RSEQMODE];
            dusk_fit(12,67,216,&FONT_T,modes[(unsigned)mode<4u?mode:0],D_ACC);
            cv_text(150,67,&FONT_T,"ALGO MODE",D_DIM);
        }else if(ui.view==V_FM){
            int p=proj.par[P_VAPRESET];
            dusk_fit(12,67,216,&FONT_T,va_preset_name(p),D_ACC);
            cv_text(184,67,&FONT_T,p==0?"PAD":p<=20?"PLUCK":"BASS",D_DIM);
        }else if(live_dest) dusk_fm_readout(live_dest,live_value);
        else dusk_fit(12,67,216,&FONT_T,plat_ms()<ui.msg_until?ui.msg:piece_name(proj.piece),plat_ms()<ui.msg_until?D_ACC:D_DIM);
        cv_line(12,82,227,82,D_ACC);
    }
    if(ui.help){
        const char *lines[HELP_LINES];int n=help_lines(ui.help-1,lines);
        dusk_fit(12,91,216,&FONT_B,BTN_LABEL[ui.help-1],D_ACC);
        for(int j=0;j<n;j++)dusk_fit(12,114+j*14,216,&FONT_XS,lines[j]+(lines[j][0]==' '),D_TEXT);
    }else if(ui.view==V_PERF)dusk_perf();
    else for(int j=0;j<4;j++)dusk_cell(j,94+(j/2)*63,dusk_knob(j),dusk_label(j,dusk_knob(j)),0);
    cv_line(12,217,227,217,D_RULE);
    if (ui.view == V_PERF && !ui.help) return;
    if (ui.view == V_NATURE) {
        static const char *const weather[] = {"R OFF","R FADE IN","R WET","R FADE OUT","R DRY"};
        uint32_t status = nature_status;
        char calls[] = "CALLS 0/2";
        calls[6] += (char)NATURE_BIRDS(status);
        cv_text(12,224,&FONT_T,NATURE_WATER_ON(status) ? "RIVER ON" : "RIVER OFF",D_DIM);
        cv_text(76,224,&FONT_T,weather[NATURE_RAIN_PHASE(status)],D_DIM);
        const char *footer = en_nature_calls() == 10u ? calls : "BANK 7/10";
        cv_text(228-text_w(&FONT_T,footer),224,&FONT_T,footer,D_LABEL);
        return;
    }
    /* Existing root + scale remain visible without an animated chord display. */
    unsigned n=0;const char *s=EN_NOTE_NAME[proj.par[P_ROOT]];
    while(*s && n<sizeof root-1)root[n++]=*s++;
    root[n++]=' ';s=mode_name();while(*s && n<sizeof root-1)root[n++]=*s++;root[n]=0;
    dusk_fit(12,224,88,&FONT_T,root,D_DIM);
    if(ui.view==V_RINGSEQ || ui.view==V_FMSEQ){
        cv_text(104,224,&FONT_T,dusk_bank()==1?"BANK 1":"BANK 2",D_LABEL);
    }else if((ui.view==V_FMFILTER || ui.view==V_PAD2FILTER) || ui.view==V_MOD || ui.view==V_FUNITOPT ||
             (grid_page(ui.view) && !wide_grid_page(ui.view)))
        cv_text(104,224,&FONT_T,dusk_bank()==2?"ROW 2":"ROW 1",D_LABEL);
    else{
        /* Count only pages on this physical button, keeping repeated presses discoverable. */
        int count=0,index=0;for(int j=0;j<NVIEWS;j++)if(VIEW_BTN[j]==VIEW_BTN[ui.view]){count++;if(j<=ui.view)index++;}
        char *p=b;itoa_u((unsigned)index,p);while(*p)p++;*p++='/';itoa_u((unsigned)count,p);
        cv_text(108,224,&FONT_T,b,D_LABEL);
    }
    s=en_frozen?"FREEZE":en_playing?"PLAY":"STOP";
    cv_text(228-text_w(&FONT_T,s),224,&FONT_T,s,D_ACC);
}
static void dusk_draw(void)
{
    uint32_t h=2166136261u,now=plat_ms();
    h=hash(h,ui.view);h=hash(h,ui.help);h=hash(h,dusk_bank());
    if(ui.view==V_RINGSEQ || ui.view==V_FMSEQ)h=hash(h,proj.par[ui.view==V_FMSEQ?P_FMSEQMODE:P_RSEQMODE]);
    h=hash(h,proj.piece);h=hash(h,proj.pool);h=hash(h,proj.par[P_ROOT]);h=hash(h,proj.par[P_MODE]);
    h=hash(h,en_playing | ((uint32_t)en_frozen<<1));h=hash(h,(uint32_t)plat_battery_pct());
    h=hash(h,now<ui.touch_until?(unsigned)(ui.touched+1):0);
    if(now<ui.msg_until){for(unsigned j=0;ui.msg[j];j++)h=hash(h,(uint8_t)ui.msg[j]);}
    for(int j=0;j<4;j++){int k=dusk_knob(j);h=hash(h,k);if(k!=K_NONE)h=hash(h,knob_get(k));}
    /* Fault/status can change the worker text without changing its numeric state. */
    h=hash(h,dc_fault());
    if (ui.view == V_NATURE) {
        h=hash(h,(uint32_t)proj.par[P_NATURE_DENSITY]);
        h=hash(h,nature_status);
    }
    if(ui.view==V_PERF)h=hash(h,now/500u);
    int dest=dusk_fm_destination();
    static uint32_t live_at;
    static int last_value;
    h=hash(h,(unsigned)dest);
    if(ui.sig[0]==h) {
        if (dest && now-live_at>=125u) {
            live_at=now;
            int value=dusk_fm_live(dest);
            if (value!=last_value) {
                last_value=value;
                cv_begin(240,16,D_BG);cv_oy=-65;
                dusk_fm_readout(dest,value);cv_blit(0,65);
            }
        }
        return;
    }
    live_at=now;
    last_value=dest ? dusk_fm_live(dest) : 0;
    ui.sig[0]=h;
    for(int band=0;band<240;band+=120){
        cv_begin(240,120,D_BG);cv_oy=-band;
        dusk_paint(dest,last_value);cv_blit(0,(uint32_t)band);
    }
}
