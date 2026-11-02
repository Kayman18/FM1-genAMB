from pathlib import Path
p=Path(__file__).parent/'app/ui.c'
s=p.read_text()
def r(a,b):
 global s
 assert a in s,a[:120]
 s=s.replace(a,b,1)
# appended view preserves all existing numeric IDs
r('V_FPROTOPT, NVIEWS };','V_FPROTOPT, V_FMFILTER, NVIEWS };')
r('"FM Proteus Options"};','"FM Proteus Options", "FM Filter"};')
r('B_FX, B_SEQ, B_SEL};','B_FX, B_SEQ, B_SEL, B_SEL};')
r('K_FPROTNEW, K_NONE };','K_FPROTNEW, K_MOD_SHAPE, K_NONE };')
r('    {P_FPROTRANGE, K_FPROTNEW, P_FMOCT, P_FMRANGE},\n};', '    {P_FPROTRANGE, K_FPROTNEW, P_FMOCT, P_FMRANGE},\n    {P_FFILTERTYPE,P_FFILTERCUT,P_FFILTERRES,P_FFILTERMIX},\n};')
r('    {"Range", "New Melody", "Octave", "Oct Drift"},\n};', '    {"Range", "New Melody", "Octave", "Oct Drift"},\n    {"Type", "Cutoff", "Resonance", "Mix"},\n};')
r('    uint8_t mod_route;\n', '    uint8_t mod_route;\n    uint8_t mod_row, fm_filter_row; /* SELECT row 1/2 for M7..M11 and FM Filter */\n')
r('static int fm_view(int v) { return (v >= V_FM && v <= V_FMEPATTERN) || v == V_FPROTOPT; }','static int fm_view(int v) { return (v >= V_FM && v <= V_FMEPATTERN) || v == V_FPROTOPT || v == V_FMFILTER; }')
# row-dependent binding for rotary encoder, rendering, accessibility
needle='static int mod_param(int pseudo)'
insert='''static int fm_filter_knob(int row,int slot) {
    static const int upper[4]={P_FFILTERTYPE,P_FFILTERCUT,P_FFILTERRES,P_FFILTERMIX};
    return row==2 ? slot==0?P_FFILTERMODEL:K_NONE : upper[slot];
}
static int mod_view_knob(int row,int slot) {
    if (ui.mod_route >= 6 && row == 2) return slot == 0 ? K_MOD_SHAPE : K_NONE;
    return VIEW_KNOB[V_MOD][slot];
}
static int bound_view_knob(int view,int slot) {
    if(view==V_FMFILTER)return fm_filter_knob(ui.fm_filter_row,slot);
    if(view==V_MOD)return mod_view_knob(ui.mod_row,slot);
    return VIEW_KNOB[view][slot];
}

'''
r(needle,insert+needle)
start=s.index('static int mod_param(int pseudo)')
end=s.index('static int knob_get(int k)',start)
s=s[:start]+'''static int mod_param(int pseudo)
{
    static const uint8_t A[11]={P_M1DEPTH,P_M2DEPTH,P_M3DEPTH,P_M4DEPTH,P_M5DEPTH,P_M6DEPTH,
                  P_M7DEPTH,P_M8DEPTH,P_M9DEPTH,P_M10DEPTH,P_M11DEPTH};
    static const uint8_t S[11]={P_M1SPEED,P_M2SPEED,P_M3SPEED,P_M4SPEED,P_M5SPEED,P_M6SPEED,
                  P_M7SPEED,P_M8SPEED,P_M9SPEED,P_M10SPEED,P_M11SPEED};
    static const uint8_t D[3]={P_M7DEST,P_M8DEST,P_M9DEST};
    static const uint8_t SH[5]={P_M7SHAPE,P_M8SHAPE,P_M9SHAPE,P_M10SHAPE,P_M11SHAPE};
    int r=ui.mod_route<11?ui.mod_route:0;
    if(pseudo==K_MOD_DEPTH)return A[r];
    if(pseudo==K_MOD_SPEED)return S[r];
    if(pseudo==K_MOD_SHAPE)return r>=6?SH[r-6]:-1;
    return r>=6&&r<9 ? D[r-6] : -1;
}

'''+s[end:]
r('    if (k == K_MOD_DEST) { int p = mod_param(k); return p < 0 ? 0 : proj.par[p]; }', '    if (k == K_MOD_DEST) { int p = mod_param(k); return p < 0 ? 0 : proj.par[p]; }')
r('    if (k == K_MOD_DEPTH || k == K_MOD_SPEED) return proj.par[mod_param(k)];','    if (k == K_MOD_DEPTH || k == K_MOD_SPEED || k == K_MOD_SHAPE) {int p=mod_param(k);return p<0?0:proj.par[p];}')
r('    if (k < P_NPARAMS) {\n        *lo = en_param_info(k)->lo;', '    if (k < P_NPARAMS) {\n        if(k==P_FFILTERTYPE && proj.par[P_FFILTERMODEL]>=1 && proj.par[P_FFILTERMODEL]<=5) {\n            *lo=*hi=proj.par[P_FFILTERMODEL]==2?1:0;return;\n        }\n        *lo = en_param_info(k)->lo;')
r('    } else if (k == K_MOD_ROUTE) {\n        *lo = 0; *hi = 8;', '    } else if (k == K_MOD_ROUTE) {\n        *lo = 0; *hi = 10;')
r('    } else if (k == K_MOD_DEPTH || k == K_MOD_SPEED) {\n        const en_param_t *p = en_param_info(mod_param(k)); *lo = p->lo; *hi = p->hi;', '    } else if (k == K_MOD_DEPTH || k == K_MOD_SPEED || k == K_MOD_SHAPE) {\n        int i=mod_param(k); if(i>=0){const en_param_t *p=en_param_info(i);*lo=p->lo;*hi=p->hi;}else{*lo=*hi=0;}')
r('    if (k == K_MOD_SPEED) return "Speed";', '    if (k == K_MOD_SPEED) return "Speed";\n    if (k == K_MOD_SHAPE) return "Shape";')
r("        b[0] = 'M'; b[1] = (char)('1' + v); b[2] = 0;", '''        b[0]='M';if(v<9){b[1]=(char)('1'+v);b[2]=0;}
        else{b[1]='1';b[2]=(char)('0'+v-9);b[3]=0;}''')
r('        int mp=mod_param(k); if(mp<0){ const char *x="Pad fixed"; while((*b++=*x++)); } else en_param_text(mp,v,b);', '''        int mp=mod_param(k);
        if(ui.mod_route>=9) {const char *x=ui.mod_route==9?"FM Cutoff":"FM Res";while((*b++=*x++));}
        else if(mp<0){ const char *x="Pad fixed"; while((*b++=*x++)); } else en_param_text(mp,v,b);''')
r('    } else if (k == K_MOD_DEPTH || k == K_MOD_SPEED) {\n        en_param_text(mod_param(k), v, b);', '    } else if (k == K_MOD_DEPTH || k == K_MOD_SPEED || k == K_MOD_SHAPE) {\n        int mp=mod_param(k);if(mp>=0)en_param_text(mp,v,b);else b[0]=0;')
r('        ui.mod_route = (uint8_t)v;', '        ui.mod_route = (uint8_t)v;\n        ui.mod_row=1;\n        ui.sig[1]=ui.sig[2]=0;')
r('    } else if (k == K_MOD_DEPTH || k == K_MOD_SPEED) {\n        int p = mod_param(k); proj.par[p] = (int16_t)v; en_set(p, v);', '''    } else if (k == K_MOD_DEPTH || k == K_MOD_SPEED || k == K_MOD_SHAPE) {
        int p=mod_param(k);if(p>=0){proj.par[p]=(int16_t)v;en_set(p,v);}''')
r('    } else if (k < P_NPARAMS) {\n        proj.par[k] = (int16_t)v; en_set(k, v);', '''    } else if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v; en_set(k, v);
        if(k==P_FFILTERMODEL) {
            int type=(v==2)?1:0;
            proj.par[P_FFILTERTYPE]=(int16_t)type;en_set(P_FFILTERTYPE,type);
        }''')
# FM filter screen belongs to SEL nav, preserving motion
r('set_view(ui.view == V_FM ? V_FMMOTION : ui.view == V_FMMOTION ? V_FMSEQ :', 'set_view(ui.view == V_FM ? V_FMMOTION : ui.view == V_FMMOTION ? V_FMFILTER :\n                 ui.view == V_FMFILTER ? V_FMSEQ :')
r('        if (ui.view == V_RINGSEQ || ui.view == V_FMSEQ) {', '''        if(ui.view==V_FMFILTER) {
            ui.fm_filter_row=(uint8_t)(((int)(ui.fm_filter_row?ui.fm_filter_row:1)-1+e%2+2)%2+1);
            ui.sig[1]=ui.sig[2]=0;
            say("SELECT",ui.fm_filter_row==1?"ROW 1":"ROW 2 MODEL");
        } else if(ui.view==V_MOD && ui.mod_route>=6) {
            ui.mod_row=(uint8_t)(((int)(ui.mod_row?ui.mod_row:1)-1+e%2+2)%2+1);
            ui.sig[1]=ui.sig[2]=0;
            say("SELECT",ui.mod_row==1?"ROW 1":"ROW 2 SHAPE");
        } else if (ui.view == V_RINGSEQ || ui.view == V_FMSEQ) {''')
r('                VIEW_KNOB[ui.view][i];\n            turn(EN_K1 + (int)i,k,e);','                bound_view_knob(ui.view,(int)i);\n            turn(EN_K1 + (int)i,k,e);')
# bring in compact common filter + mod cells before draw_main
marker='static void draw_main(void)'
functions='''static void fm_filter_cell(int slot,int row,int y,int ht) {
    int k=fm_filter_knob(row,slot), x=4+59*slot;
    int active=ui.fm_filter_row==row;
    uint16_t edge=active?K_FM:mix(K_LINE,K_BG,4);
    outline_box(x,y,slot==3?55:56,ht,edge,active?mix(K_PANEL,K_FM,2):K_PANEL);
    if(k==K_NONE)return;
    const char *label=row==2?"Model":KNOB_LABEL[V_FMFILTER][slot];
    char value[20]; knob_text(k,value);
    cv_text(x+3,y+3,&FONT_T,label,active?K_TEXT:K_DIM);
    const felucca_font_t *f=fit_value_font(value,50);
    if(text_w(f,value)>49)f=&FONT_T;
    text_c(x+28,y+29,f,value,K_TEXT);
    text_c(x+28,y+ht-14,&FONT_T,slot==0?"K1":slot==1?"K2":slot==2?"K3":"K4",active?K_FM:K_DIM);
    if(active)cv_rect(x+4,y+ht-3,47,2,K_FM);
}
static void mod_shape_cell(int slot,int row,int y,int ht) {
    int k=mod_view_knob(row,slot),x=4+59*slot,active=ui.mod_row==row;
    uint16_t col=page_colour(V_MOD);
    outline_box(x,y,slot==3?55:56,ht,active?col:mix(K_LINE,K_BG,4),active?mix(K_PANEL,col,2):K_PANEL);
    if(k==K_NONE)return;
    char val[20];knob_text(k,val);
    if(k==K_MOD_SHAPE && ui.mod_route==6) { /* avoid tiny truncated values */ }
    cv_text(x+3,y+3,&FONT_T,row==2?"Shape":KNOB_LABEL[V_MOD][slot],active?K_TEXT:K_DIM);
    const felucca_font_t *f=fit_value_font(val,50);
    if(text_w(f,val)>49)f=&FONT_T;
    text_c(x+28,y+30,f,val,K_TEXT);
    text_c(x+28,y+ht-14,&FONT_T,slot==0?"K1":slot==1?"K2":slot==2?"K3":"K4",active?col:K_DIM);
    if(active)cv_rect(x+4,y+ht-3,47,2,col);
}
/* Visual approximation of the active analog model's magnitude response.
 * This display is never evaluated in the audio interrupt. */
static int filter_response_y(int x) {
    int model=proj.par[P_FFILTERMODEL],type=proj.par[P_FFILTERTYPE];
    int cut=30+(proj.par[P_FFILTERCUT]*177)/100;
    int d=x-cut;
    if(model==2)type=1;
    else if(model==6 && type==3)type=3;
    else if(model>=1&&model<=5)type=0;
    if(model==0&&type==3)return 28;
    if(type==1) return d<-20?46:d>20?22:46-(d+20)*24/40;
    if(type==2) {int a=d<0?-d:d;return a<24?23: a>70?46:23+(a-24)*23/46;}
    if(model==6&&type==3){int a=d<0?-d:d;return a<15?46:a>60?24:46-(a-15)*22/45;}
    return d<0?24:d>75?47:24+d*23/75;
}

'''
r(marker,functions+marker)
# main draw hash for those new params
r('    if (ui.view == V_MOD) {\n        h = hash(h, ui.frame / 4);', '''    if (ui.view == V_FMFILTER) {
        h=hash(h,ui.fm_filter_row);
        for(int p=P_FFILTERMODEL;p<=P_FFILTERMIX;p++)h=hash(h,proj.par[p]);
    }
    if (ui.view == V_MOD) {
        h = hash(h, ui.mod_row);
        if(ui.mod_route>=6) {
            for(int i=0;i<4;i++)h=hash(h,(uint32_t)knob_get(mod_view_knob(1,i)));
            h=hash(h,(uint32_t)knob_get(K_MOD_SHAPE));
        }
        h = hash(h, ui.frame / 4);''')
r('    /* The Mod page puts its controls immediately below the header. */', '''    if(ui.view==V_FMFILTER) {
        uint16_t col=K_FM;
        outline_box(2,1,236,54,col,K_PANEL);
        cv_text(8,3,&FONT_T,"FILTER RESPONSE",col);
        cv_text(176,3,&FONT_T,proj.par[P_FFILTERMODEL]>=1&&proj.par[P_FFILTERMODEL]<=4?"24 dB/OCT":"12 dB/OCT",K_DIM);
        for(int x=20;x<=220;x+=40)cv_line(x,19,x,49,K_LINE);
        cv_line(20,34,220,34,K_PANEL);
        int py=filter_response_y(20);
        for(int x=21;x<=220;x+=2){int y=filter_response_y(x);cv_line(x-1,py,x,y,col);py=y;}
        cv_text(19,43,&FONT_T,"50Hz",K_DIM);cv_text(97,43,&FONT_T,"1k",K_DIM);
        cv_text(145,43,&FONT_T,"4k",col);cv_text(196,43,&FONT_T,"16k",K_DIM);
        text_c(120,57,&FONT_T,ui.fm_filter_row==1?"SELECT: ROW 1  KNOBS 1-4":"SELECT: ROW 2  K1 MODEL",K_DIM);
        for(int i=0;i<4;i++)fm_filter_cell(i,1,70,59);
        cv_blit(0,MAIN_Y);return;
    }
    if(ui.view==V_MOD && ui.mod_route>=6) {
        int row=ui.mod_route,rp=mod_param(K_MOD_SHAPE),shape=rp>=0?proj.par[rp]:0;
        uint16_t col=page_colour(V_MOD);
        outline_box(2,1,236,60,col,K_PANEL);
        cv_text(8,4,&FONT_T,row==9?"M10 FM CUTOFF":row==10?"M11 FM RESONANCE":row==6?"M7 MOD SOURCE":row==7?"M8 MOD SOURCE":"M9 MOD SOURCE",col);
        for(int x=10;x<=230;x+=44)cv_line(x,19,x,45,K_LINE);
        cv_line(10,32,230,32,K_LINE);
        /* Waveform preview; random shapes show a deterministic example contour. */
        int lastx=11,lasty=32;
        static const int rnd[9]={-8,17,-12,9,-18,16,4,-6,12};
        for(int x=13;x<=229;x+=2) {
            int q=x-11,seg=q/27;
            float ph=(float)q/218.0f;int y=32;
            if(shape==0){int i=seg>7?7:seg;float t=(float)(q-i*27)/27.0f;
                t=t*t*(3.0f-2.0f*t);y=32-(int)(rnd[i]*(1.0f-t)+rnd[i+1]*t);}
            else if(shape==1) y=32-rnd[seg>8?8:seg];
            else if(shape==2) y=32-(int)(19.0f*sine(ph));
            else if(shape==3) y=32-(ph<0.25f?19:-19);
            else if(shape==4) y=32-(int)(19.0f*(ph<0.5f?4.0f*ph-1.0f:3.0f-4.0f*ph));
            else y=32-(int)(19.0f*(2.0f*ph-1.0f));
            cv_line(lastx,lasty,x,y,col);lastx=x;lasty=y;
        }
        text_c(120,49,&FONT_T,ui.mod_row==1?"SELECT: ROW 1  KNOBS 1-4":"SELECT: ROW 2  SHAPE",K_DIM);
        for(int i=0;i<4;i++)mod_shape_cell(i,1,65,63);
        cv_blit(0,MAIN_Y);return;
    }
    /* The Mod page puts its controls immediately below the header. */''')
# bottom draw new special cases
r('    if (ui.view==V_RINGSEQ || ui.view==V_FMSEQ) {\n        int row=ui.seq_zone[ui.view==V_FMSEQ];', '''    if(ui.view==V_MOD && ui.mod_route>=6)h=hash(h,ui.mod_row);
    if(ui.view==V_FMFILTER)h=hash(h,ui.fm_filter_row);
    if (ui.view==V_RINGSEQ || ui.view==V_FMSEQ) {
        int row=ui.seq_zone[ui.view==V_FMSEQ];''')
r('    if (ui.view==V_RINGSEQ || ui.view==V_FMSEQ) {\n        for(int slot=0;slot<4;++slot)', '''    if (ui.view==V_FMFILTER) {
        for(int i=0;i<4;i++)fm_filter_cell(i,2,4,64);
        cv_blit(0,knob_y);return;
    }
    if(ui.view==V_MOD && ui.mod_route>=6) {
        for(int i=0;i<4;i++)mod_shape_cell(i,2,4,64);
        cv_blit(0,knob_y);return;
    }
    if (ui.view==V_RINGSEQ || ui.view==V_FMSEQ) {
        for(int slot=0;slot<4;++slot)''')
# Bottom MOD row1 drawn in MAIN; draw_knobs special case fine.
# General UI in non custom when view==mod for M1-6, unchanged
# refresh hash MAIN for FM filter, MOD
r('    if (fm_view(ui.view)) {\n        for (i = P_FMLEVEL;', '    if (fm_view(ui.view)) {\n        for (i = P_FMLEVEL;')
# SCREEN HELP and words
r('    case B_SEL: L("FM - SIX OPERATORS"); L("PATCH, LEVEL, VOICES, GATE"); L("AGAIN: MOTION / SEQUENCER");','    case B_SEL: L("FM - SIX OPERATORS"); L("PATCH, LEVEL, VOICES, GATE"); L("AGAIN: MOTION / FILTER / SEQ");')
# UI init row defaults
if 'ui.seq_zone[0]' in s:
 at=s.index('void ui_init(void)'); print(s[at:at+470])
p.write_text(s)
