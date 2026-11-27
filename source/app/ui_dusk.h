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
    if(v==V_MOD && ui.mod_route<EN_MOD_COUNT)return ui.mod_row==2?2:1;
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
#include "assets/brushed_knob.h"
static void dusk_aluminum_knob(int cx,int cy,int v,int lo,int hi)
{
    float pos=hi>lo?(float)(v-lo)/(hi-lo):0;
    for(int y=0;y<39;y++){
        int py=cy+y-18;
        if(py+cv_oy<0 || py+cv_oy>=120)continue;
        for(int x=0;x<39;x++){
            int g=brushed_cap[y][x];
            if(g)cv_pset(cx+x-19,py,RGB(g,g,g+3));
        }
    }
    for(int i=0;i<=24;i++){
        float a=(0.375f+i/32.f)*FM_TWO_PI;
        cv_rect(cx+(int)(21*fm_sinf(a+FM_TWO_PI*.25f)),cy+(int)(21*fm_sinf(a)),2,2,i<=24*pos?D_ACC:D_RULE);
    }
    float a=(0.375f+.75f*pos)*FM_TWO_PI;
    int dx=(int)(15*fm_sinf(a+FM_TWO_PI*.25f)),dy=(int)(15*fm_sinf(a));
    cv_line(cx+dx/3,cy+dy/3,cx+dx,cy+dy,D_BG);
    cv_line(cx+dx/3+1,cy+dy/3,cx+dx+1,cy+dy,D_BG);
}
/* Prefer the largest face that fits the available width. */
static const felucca_font_t *dusk_font(const char *s,int w,int large)
{
    if(large && text_w(&FONT_L,s)<=w)return &FONT_L;
    if(large && text_w(&FONT_M,s)<=w)return &FONT_M;
    if(text_w(&FONT_S,s)<=w)return &FONT_S;
    if(text_w(&FONT_XS,s)<=w)return &FONT_XS;
    return &FONT_T;
}

static void dusk_cell(int slot,int y,int k,const char *label,int full)
{
    int x=12+(slot%2)*118,lo,hi,v,p=0;
    char l[48],value[64],kn[3]={'K',(char)('1'+slot),0};
    const felucca_font_t *vf=&FONT_M;
    int hot=plat_ms()<ui.touch_until && ui.touched==slot;
    cv_text(x,y,&FONT_XS,kn,hot?D_ACC:D_LABEL);
    if(k==K_NONE){cv_text(x,y+20,&FONT_S,"--",D_RULE);return;}
    if(full && ui.view==V_MOD && slot==0){
        char name[12]; knob_text(K_MOD_ROUTE,name);
        char *dst=l;const char *src=name;while(*src)*dst++=*src++;
        *dst++=' ';caps(dst,label,sizeof l-(unsigned)(dst-l));
    }else caps(l,label,sizeof l);
    dusk_fit(x+21,y,87,dusk_font(l,87,0),l,D_LABEL);
    knob_text(k,value);
    const char *shown=value;
    if(full && k==P_VAPRESET)shown=va_preset_name(knob_get(k));
    if(full && k==K_MOD_SHAPE){
        static const char *const shapes[]={"Smooth Random","Stepped Random","Sine","Pulse","Triangle","Saw","Triple Sloth"};
        int shape=knob_get(k);
        if((unsigned)shape<7u)shown=shapes[shape];
    }
    if(full){
        knob_range(k,&lo,&hi);
        dusk_aluminum_knob(x+49,y+40,knob_get(k),lo,hi);
        vf=dusk_font(shown,105,1);
        dusk_fit(x+((105-text_w(vf,shown))>0?(105-text_w(vf,shown))/2:0),y+63,105,vf,shown,D_TEXT);
        return;
    }
    vf=dusk_font(shown,105,1);
    dusk_fit(x,y+22,105,vf,shown,D_TEXT);
    knob_range(k,&lo,&hi);v=knob_get(k);
    if(hi>lo){p=(v-lo)*10/(hi-lo);if(p<0)p=0;if(p>10)p=10;}
    for(int j=0;j<10;j++)cv_rect(x+j*9,y+58,6,2,j<p?D_ACC:D_RULE);
}
static void dusk_diagnostics(void)
{
    perf_stats_t s;char value[24];perf_read(&s);
    const char *names[]={"AUDIO AVG %","AUDIO PEAK %","DROPOUTS","FAULTS",
        "CPU0 US","CPU0 PEAK US","CPU1 WORK US","WAIT US",
        "PK C0 US","PK C1 US","PK WAIT US","PK TOTAL US"};
    unsigned active=s.worst_total_us>=s.worst_wait_us?s.worst_total_us-s.worst_wait_us:0;
    uint32_t vals[]={s.audio_pct,s.peak_pct,s.dropouts,s.faults,
        s.cpu0_us,s.cpu0_peak_us,s.worker_us,s.wait_us,
        active,s.worst_work_us,s.worst_wait_us,s.worst_total_us};
    for(int j=0;j<4;j++){
        int i=4*(ui.perf_bank-1)+j,x=12+118*(j%2),y=48+96*(j/2);
        dusk_fit(x,y,105,dusk_font(names[i],105,0),names[i],D_LABEL);
        itoa_u(vals[i],value);dusk_fit(x,y+30,105,dusk_font(value,105,1),value,D_TEXT);
    }
}

static void dusk_lfo(void)
{
    static const char *labels[]={"K1 Shape","K2 Destination","K3 Depth","K4 Speed"};
    int shape=knob_get(K_MOD_SHAPE);
    for(int i=0;i<=4;i++)cv_line(12+54*i,47,12+54*i,76,D_RULE);
    cv_line(12,61,228,61,D_RULE);
    int previous=61, head=en_mod_trace_head, count=en_mod_trace_count;
    for(int i=0;i<=108;i++){
        float t=i/108.f,w;
        if(shape==2)w=fm_sinf(FM_TWO_PI*t);
        else if(shape==3)w=t<0.25f?1.f:-1.f;
        else if(shape==4)w=t<0.5f?4*t-1:3-4*t;
        else if(shape==5)w=2*t-1;
        else {
            int idx=(head+EN_MOD_TRACE-count+(count?i*(count-1)/108:0))%EN_MOD_TRACE;
            w=count?en_mod_trace[ui.mod_route][idx]/127.f:0;
        }
        int y=61-(int)(13*w);
        if(i){cv_line(10+2*i,previous,12+2*i,y,D_ACC);cv_line(10+2*i,previous+1,12+2*i,y+1,D_ACC);}
        previous=y;
    }
    cv_line(120,84,120,233,D_RULE);
    cv_line(10,158,230,158,D_RULE);
    for(int i=0;i<4;i++){
        int x=6+120*(i%2), y=83+79*(i/2),k=dusk_knob(i),lo,hi;
        cv_text(x+1,y,dusk_font(labels[i],114,0),labels[i],D_LABEL);
        knob_range(k,&lo,&hi);
        dusk_aluminum_knob(x+22,y+47,knob_get(k),lo,hi);
        char value[32];knob_text(k,value);
        if(k==K_MOD_DEPTH){int j=0;while(value[j])j++;value[j++]='%';value[j]=0;}
        if(k==K_MOD_SHAPE){
            static const char *const names[]={"Smooth Random","Stepped Random","Sine","Pulse","Triangle","Saw","Triple Sloth"};
            const char *src=names[shape];char *dst=value;
            while((*dst++=*src++)){}
        }
        char *second=0;
        for(char *ch=value;*ch;++ch)if(*ch==' '){*ch=0;second=ch+1;break;}
        const char *longer=second && text_w(&FONT_M,second)>text_w(&FONT_M,value)?second:value;
        const felucca_font_t *font=dusk_font(longer,66,1);
        dusk_fit(x+46,y+(second?24:35),66,font,value,D_TEXT);
        if(second)dusk_fit(x+46,y+47,66,font,second,D_TEXT);
    }
}

static void dusk_paint(void)
{
    char title[48],b[24],root[48];const felucca_font_t *tf=&FONT_L;
    int bp=plat_battery_pct();
    cv_text(9,25,&FONT_XS,"genAMB",D_LABEL);
    if(bp<0){b[0]='-';b[1]='-';b[2]=0;}
    else if(bp>100){b[0]='U';b[1]='S';b[2]='B';b[3]=0;}
    else{char *p=b;itoa_u((uint32_t)bp,p);while(*p)p++;*p++='%';*p=0;}
    cv_text(231-text_w(&FONT_T,b),7,&FONT_T,b,D_DIM);
    caps(title,ui.view==V_MOD?"LFO":VIEW_NAME[ui.view],sizeof title);
    if(ui.view==V_MOD){title[3]=' ';title[4]='-';title[5]=' ';knob_text(K_MOD_ROUTE,title+6);}
    tf=text_w(&FONT_M,title)<=187?&FONT_M:dusk_font(title,187,0);
    dusk_fit(9,1,187,tf,title,D_TEXT);
    if (ui.view == V_NATURE) {
        unsigned density = (unsigned)proj.par[P_NATURE_DENSITY];
        char *end = b;
        itoa_u(density, b);
        while (*end) ++end;
        *end++ = '%'; *end = 0;
        cv_text(70,25,&FONT_T,"SELECT: ANIMAL DENSITY",D_LABEL);
        cv_text(228-text_w(&FONT_XS,b),24,&FONT_XS,b,D_ACC);

    } else if(ui.view!=V_PERF && ui.view!=V_MOD){
        if(ui.view==V_RINGSEQ || ui.view==V_FMSEQ){
            static const char *const modes[]={"Original","Hallucigenia","Proteus","Unit-D"};
            int mode=proj.par[ui.view==V_FMSEQ?P_FMSEQMODE:P_RSEQMODE];
            dusk_fit(70,25,105,&FONT_T,modes[(unsigned)mode<4u?mode:0],D_ACC);
            cv_text(166,25,&FONT_T,"ALGO MODE",D_DIM);
        }else if(ui.view==V_FM){
            int p=proj.par[P_VAPRESET];
            dusk_fit(70,25,115,&FONT_T,va_preset_name(p),D_ACC);
            cv_text(194,25,&FONT_T,p==0?"PAD":p<=20?"PLUCK":"BASS",D_DIM);
        }
        else dusk_fit(70,25,158,&FONT_T,plat_ms()<ui.msg_until?ui.msg:piece_name(proj.piece),plat_ms()<ui.msg_until?D_ACC:D_DIM);

    }
    cv_line(9,41,231,41,D_ACC);
    if(ui.help){
        const char *lines[HELP_LINES];int n=help_lines(ui.help-1,lines);
        dusk_fit(12,49,216,&FONT_B,BTN_LABEL[ui.help-1],D_ACC);
        for(int j=0;j<n;j++)dusk_fit(12,72+j*14,216,&FONT_XS,lines[j]+(lines[j][0]==' '),D_TEXT);
    }else if(ui.view==V_MOD){dusk_lfo();return;}
    else if(VIEW_BTN[ui.view]!=B_HOME){
        if(ui.view==V_PERF && ui.perf_bank)dusk_diagnostics();
        else for(int j=0;j<4;j++)dusk_cell(j,48+96*(j/2),dusk_knob(j),dusk_label(j,dusk_knob(j)),1);
        return;
    }else for(int j=0;j<4;j++)dusk_cell(j,48+(j/2)*83,dusk_knob(j),dusk_label(j,dusk_knob(j)),0);
    if(VIEW_BTN[ui.view]!=B_HOME && ui.view!=V_MOD)return;
    cv_line(12,217,227,217,D_RULE);
    /* Existing root + scale remain visible without an animated chord display. */
    unsigned n=0;const char *s=EN_NOTE_NAME[proj.par[P_ROOT]];
    while(*s && n<sizeof root-1)root[n++]=*s++;
    root[n++]=' ';s=mode_name();while(*s && n<sizeof root-1)root[n++]=*s++;root[n]=0;
    dusk_fit(12,224,88,&FONT_T,root,D_DIM);
    if(ui.view==V_RINGSEQ || ui.view==V_FMSEQ){
        cv_text(104,224,&FONT_T,dusk_bank()==1?"BANK 1":"BANK 2",D_LABEL);
    }else if(ui.view==V_MOD)cv_text(93,224,&FONT_T,"SELECT: LFO",D_LABEL);
    else if((ui.view==V_FMFILTER || ui.view==V_PAD2FILTER) || ui.view==V_MOD || ui.view==V_FUNITOPT ||
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
    if(ui.view==V_MOD){
        h=hash(h,ui.mod_route);
        int shape=knob_get(K_MOD_SHAPE);
        if(shape<2 || shape==6)h=hash(h,en_mod_trace_head);
    }
    if(ui.view==V_PERF)h=hash(h,ui.perf_bank);
    if(ui.sig[0]==h)return;
    ui.sig[0]=h;
    for(int band=0;band<240;band+=120){
        cv_begin(240,120,D_BG);cv_oy=-band;
        dusk_paint();cv_blit(0,(uint32_t)band);
    }
}
