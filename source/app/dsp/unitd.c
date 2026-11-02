/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 genAMB adaptation contributors.
 * Based on Animatek's UNIT-D / UnitDistanceSeq.cpp, GPL-3.0-or-later:
 * https://github.com/animatek/UZZ-VCV-RACK/blob/main/src/UnitDistanceSeq.cpp
 *
 * Hardware adaptation: fixed arrays instead of VCV engine/GUI dependencies,
 * chord-tone pitches from the existing Original progression, one scheduling
 * clock for both Rings and FM, bounded eight-voice graph walks, no heap.
 * The walk, graph, locked loop, gate density and geometry are adapted from
 * UNIT-D's published original algorithm; X/Y/ACC are used internally for
 * pitch and velocity instead of providing new physical CV jacks.
 */
#define UD_MAGIC 0x55445431u  /* UDT1 */
#define UD_NODES 64
#define UD_POLY 8
#define UD_CAPTURE 32
#define UD_HISTORY 8

typedef struct {float x,y,nx,ny;uint64_t adj;uint8_t degree;} ud_node_t;
typedef struct {uint8_t node[UD_POLY],gate;} ud_step_t;
typedef struct {
    ud_node_t nodes[UD_NODES];
    ud_step_t capture[UD_CAPTURE],locked[UD_CAPTURE];
    uint32_t rng[UD_POLY],base_rng,gate_step;
    uint8_t v_node[UD_POLY],v_offset[UD_POLY],node_count,poly;
    uint8_t capture_pos,capture_count,locked_len,locked_pos;
    uint8_t walk_history[UD_HISTORY],history_count,history_pos;
    uint8_t first_step,was_locking,edges,max_degree;
    int8_t lock_direction;
    int16_t cached_seed,cached_nodes,cached_radius,cached_tolerance,cached_density;
} ud_state_t;
static EN_POOL ud_state_t ud_state[2];
/* Small UI snapshot in audio-pool memory. The display is non-authoritative. */
static EN_POOL volatile uint32_t ud_view_version;
volatile uint32_t ud_note_count[2],ud_chord_fail[2];
static en_ud_bank_t ud_restored;
static int ud_pending_restore;

static int ud_base(int layer) {return layer ? P_FUDSEED : P_RUDSEED;}
static int ud_clamp(int x,int a,int b){return x<a?a:x>b?b:x;}
static float ud_clampf(float x,float a,float b){return x<a?a:x>b?b:x;}
static uint32_t ud_hash(uint32_t x) {
    x^=x>>16; x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;
}
static float ud_fract(float x){return x-fm_floorf(x);}
static void ud_capture(ud_state_t *s,const uint8_t gates[UD_POLY]) {
    ud_step_t *p=&s->capture[s->capture_pos];
    for(int v=0;v<UD_POLY;v++)p->node[v]=s->v_node[v];
    p->gate=0;
    for(int v=0;v<UD_POLY;v++)if(gates[v])p->gate|=(uint8_t)(1u<<v);
    s->capture_pos=(uint8_t)((s->capture_pos+1)&31);
    if(s->capture_count<UD_CAPTURE)s->capture_count++;
}
static int ud_pick_neighbor(ud_state_t *s,int voice);
static void ud_freeze(ud_state_t *s,int n) {
    /* Restoring a project with LOCK already engaged must not create a
     * permanent silent loop. The large captured loop is not saved in the
     * 3,840-byte project flash object. Recreate a deterministic phrase from
     * the stored node/RNG and the same UNIT-D geometry. */
    if(s->capture_count==0){
        int layer=(s==&ud_state[1]),b=ud_base(layer);
        for(int step=0;step<UD_CAPTURE;step++){
            uint8_t gate[UD_POLY]={0};
            if(step)for(int v=0;v<s->poly;v++)s->v_node[v]=(uint8_t)ud_pick_neighbor(s,v);
            s->gate_step++;
            for(int v=0;v<s->poly;v++){
                int k=s->v_node[v],deg=s->nodes[k].degree,gd=par[b+7];
                if(!deg||gd<=0)continue;
                uint32_t h=ud_hash(s->base_rng^(uint32_t)k*2654435761u^
                                   s->gate_step*2246822519u^(uint32_t)v*374761393u);
                float degreeNorm=(float)deg/(float)(s->max_degree?s->max_degree:1);
                float randPart=(float)(h&0xffffffu)*(1.f/16777216.f);
                gate[v]=(uint8_t)(gd>=100 ||
                    (randPart*0.7f+(1.f-degreeNorm)*0.3f)*100.f<(float)gd);
            }
            ud_capture(s,gate);
        }
    }
    s->locked_len=(uint8_t)n;s->locked_pos=0;
    int oldest=s->capture_count<UD_CAPTURE?0:s->capture_pos;
    for(int i=0;i<n;i++){
        if(!s->capture_count){
            for(int v=0;v<UD_POLY;v++)s->locked[i].node[v]=s->v_node[v];
            s->locked[i].gate=0;
        }else s->locked[i]=s->capture[(oldest+i%s->capture_count)&31];
    }
}
static int ud_pick_connected(ud_state_t *s,int start){
    for(int k=1;k<=s->node_count;k++){
        int j=(start+k)%s->node_count;
        if(s->nodes[j].degree)return j;
    }
    return start;
}
static int ud_pick_neighbor(ud_state_t *s,int voice){
    int n=s->node_count,current=s->v_node[voice];
    uint64_t mask=s->nodes[current].adj;
    if(!s->edges)return current;
    if(!mask)return ud_pick_connected(s,(current+s->v_offset[voice])%n);
    int mode=par[ud_base((s==&ud_state[1]))+4];
    if(mode==1){
        s->rng[voice]=ud_hash(s->rng[voice]+0x9e3779b9u+(uint32_t)current+(uint32_t)voice*97u);
        int target=(int)(s->rng[voice]%s->nodes[current].degree);
        for(int i=0;i<n;i++)if(mask&(1ull<<i)){
            if(!target--)return i;
        }
    }else if(mode==2){
        int best=-1,quality=-1;
        for(int i=0;i<n;i++)if(mask&(1ull<<i)){
            int used=0;
            for(int v=0;v<voice;v++)if(s->v_node[v]==i)used=1;
            int recent=0;
            for(int j=0;j<s->history_count;j++)if(s->walk_history[j]==i)recent=1;
            int rank=(int)s->nodes[i].degree*4-(used?256:0)-(recent&&voice==0?128:0);
            if(rank>quality || best<0){best=i;quality=rank;}
        }
        if(best>=0)return best;
    }else{
        int offset=voice?1+voice+s->v_offset[voice]:1;
        for(int k=offset;k<=n+offset;k++){
            int i=(current+k)%n,used=0;
            if(!(mask&(1ull<<i)))continue;
            for(int v=0;v<voice;v++)if(s->v_node[v]==i)used=1;
            if(!used)return i;
        }
    }
    for(int i=0;i<n;i++)if(mask&(1ull<<i))return i;
    return current;
}
/* Called only when Seed/Nodes/Radius/Tol/Density change, never each sample.
 * Squared distance avoids expensive sqrt inside 64*63/2 adjacency tests. */
static void ud_graph(int layer){
    ud_state_t *s=&ud_state[layer];int b=ud_base(layer);
    int seed=par[b],count=ud_clamp(par[b+1],8,64),radius=par[b+2],density=par[b+3],tol=par[b+8];
    if(s->cached_seed==seed&&s->cached_nodes==count&&s->cached_radius==radius&&
       s->cached_density==density&&s->cached_tolerance==tol)return;
    s->cached_seed=(int16_t)seed;s->cached_nodes=(int16_t)count;
    s->cached_radius=(int16_t)radius;s->cached_density=(int16_t)density;s->cached_tolerance=(int16_t)tol;
    s->node_count=(uint8_t)count;s->edges=0;s->max_degree=0;
    float minx=10.f,miny=10.f,maxx=-10.f,maxy=-10.f;
    for(int i=0;i<count;i++){
        float a=ud_fract((float)seed*0.017f+(float)i*0.61803398875f);
        float z=ud_fract((float)seed*0.031f+(float)i*0.41421356237f);
        float t1=FM_TWO_PI*a,t2=FM_TWO_PI*z;
        ud_node_t *p=&s->nodes[i];
        p->x=fm_cosf(t1)+0.65f*fm_cosf(t2);
        p->y=fm_sinf(t1)+0.65f*fm_sinf(t2);
        if(p->x<minx)minx=p->x;
        if(p->x>maxx)maxx=p->x;
        if(p->y<miny)miny=p->y;
        if(p->y>maxy)maxy=p->y;
        p->degree=0;p->adj=0;
    }
    float sx=ud_clampf(maxx-minx,0.0001f,100.f),sy=ud_clampf(maxy-miny,0.0001f,100.f);
    for(int i=0;i<count;i++){
        s->nodes[i].nx=ud_clampf((s->nodes[i].x-minx)/sx,0.f,1.f);
        s->nodes[i].ny=ud_clampf((s->nodes[i].y-miny)/sy,0.f,1.f);
    }
    float d=(float)density*0.01f;
    float t=ud_clampf((float)tol*0.01f+0.01f+d*d*0.42f,0.01f,0.5f);
    float r=(float)radius*0.01f,lo=r>t?r-t:0.f,hi=r+t;
    float low=lo*lo,high=hi*hi;
    for(int i=0;i<count;i++)for(int j=i+1;j<count;j++){
        float dx=s->nodes[i].x-s->nodes[j].x,dy=s->nodes[i].y-s->nodes[j].y,dist=dx*dx+dy*dy;
        if(dist>low && dist<high){
            s->nodes[i].adj|=1ull<<j;s->nodes[j].adj|=1ull<<i;
            s->nodes[i].degree++;s->nodes[j].degree++;s->edges=1;
        }
    }
    for(int i=0;i<count;i++)if(s->nodes[i].degree>s->max_degree)s->max_degree=s->nodes[i].degree;
    s->base_rng=ud_hash((uint32_t)seed*73856093u^(uint32_t)(radius*10)^((uint32_t)tol<<11)^((uint32_t)density<<3));
    /* Non-forced changes keep the walker position (the underlying graph changes). */
    for(int v=0;v<UD_POLY;v++){
        s->v_node[v]=(uint8_t)(s->v_node[v]%count);
        s->v_offset[v]=(uint8_t)(ud_hash(s->base_rng^(uint32_t)(count*(v+1))^((uint32_t)v*0x85ebca6bu))%(uint32_t)count);
        if(!s->rng[v])s->rng[v]=ud_hash(s->base_rng^((uint32_t)v*0x9e3779b9u));
    }
}
static void ud_reset(int layer){
    ud_state_t *s=&ud_state[layer];
    ud_graph(layer);
    int count=s->node_count,poly=ud_clamp(par[ud_base(layer)+11],1,layer?8:4);
    s->poly=(uint8_t)poly;
    for(int v=0;v<UD_POLY;v++){
        s->rng[v]=ud_hash(s->base_rng^((uint32_t)v*0x9e3779b9u));
        s->v_node[v]=(uint8_t)(((v*(count/poly))+s->v_offset[v])%count);
    }
    s->first_step=1;s->gate_step=0;s->capture_pos=0;s->capture_count=0;
    s->locked_len=s->locked_pos=0;s->was_locking=0;
    s->history_count=s->history_pos=0;
}
static void ud_init(void){
    memset(ud_state,0,sizeof ud_state);
    memset((void*)ud_note_count,0,sizeof ud_note_count);
    memset((void*)ud_chord_fail,0,sizeof ud_chord_fail);
    ud_pending_restore=0;
    for(int layer=0;layer<2;layer++){
        ud_state_t *s=&ud_state[layer];
        s->cached_seed=s->cached_nodes=s->cached_radius=s->cached_density=s->cached_tolerance=-1;
        s->first_step=1;
        ud_reset(layer);
    }
}
void en_unitd_snapshot(en_ud_bank_t *out){
    uint32_t before,after;
    do {
        before=ud_view_version;BARRIER();
        out->magic=UD_MAGIC;
        for(int layer=0;layer<2;layer++){
            const ud_state_t *s=&ud_state[layer];
            out->seq[layer].walk=s->rng[0];out->seq[layer].step=s->gate_step;
            memcpy(out->seq[layer].node,s->v_node,UD_POLY);
        }
        BARRIER();after=ud_view_version;
    }while((before&1u)||before!=after);
}
void en_unitd_restore_boot(const en_ud_bank_t *in){
    if(in->magic!=UD_MAGIC)return;
    ud_restored=*in;
    ud_pending_restore=3;
}
static void ud_apply_restored(int layer){
    if(!(ud_pending_restore&(1<<layer)))return;
    ud_state_t *s=&ud_state[layer];
    const en_ud_save_t *src=&ud_restored.seq[layer];
    if(src->walk){
        s->rng[0]=src->walk;s->gate_step=src->step;
        for(int i=0;i<UD_POLY;i++)if(src->node[i]<s->node_count)s->v_node[i]=src->node[i];
        s->first_step=0;
    }
    ud_pending_restore &=~(1<<layer);
}
/* Called at clock rate (not at audio sample rate) on Core 0. */
static void ud_tick(int layer){
    ud_state_t *s=&ud_state[layer];int b=ud_base(layer);
    ud_view_version++; BARRIER();
    ud_graph(layer);
    if(s->poly!=(uint8_t)ud_clamp(par[b+11],1,layer?8:4))ud_reset(layer);
    ud_apply_restored(layer);
    int lock=par[b+5],locking=lock>=2||lock<=-2,dir=lock<0?-1:1;
    if(locking && (!s->was_locking||s->lock_direction!=dir))ud_freeze(s,dir<0?32:16);
    if(!locking&&s->was_locking){s->history_count=0;s->first_step=1;}
    s->was_locking=(uint8_t)locking;s->lock_direction=(int8_t)dir;
    uint8_t gate[UD_POLY]={0};
    int poly=s->poly;
    int use_lock=locking&&s->locked_len>0;
    if(use_lock){
        int pos=s->locked_pos;s->locked_pos=(uint8_t)((pos+1)%s->locked_len);
        uint32_t h=ud_hash(s->base_rng^(uint32_t)pos*1103515245u^0x51ed270bu);
        if((h%100u)<(uint32_t)(lock<0?-lock:lock)){
            ud_step_t *st=&s->locked[pos];
            for(int v=0;v<poly;v++){
                s->v_node[v]=st->node[v];gate[v]=(st->gate>>v)&1;
            }
            s->gate_step++;
        }else use_lock=0;
    }
    if(!use_lock){
        if(s->first_step)s->first_step=0;
        else {
            for(int v=0;v<poly;v++){
                int last=s->v_node[v];
                s->v_node[v]=(uint8_t)ud_pick_neighbor(s,v);
                if(v==0){s->walk_history[s->history_pos]=last;s->history_pos=(s->history_pos+1)&7;
                    if(s->history_count<UD_HISTORY)s->history_count++;}
            }
            s->gate_step++;
        }
        int gd=par[b+7];
        for(int v=0;v<poly;v++){
            int node=s->v_node[v],degree=s->nodes[node].degree;
            if(!degree||!s->edges||gd<=0)continue;
            uint32_t h=ud_hash(s->base_rng^(uint32_t)node*2654435761u^
                               (s->gate_step*2246822519u)^(uint32_t)v*374761393u);
            float randpart=(float)(h&0xffffffu)*(1.f/16777216.f);
            float degnorm=(float)degree/(float)(s->max_degree?s->max_degree:1);
            float score=randpart*0.7f+(1.f-degnorm)*0.3f;
            if(gd>=100||(score*100.f<(float)gd))gate[v]=1;
        }
        ud_capture(s,gate);
    }
    if(chord_nt>0 && par[P_MODE]!=MODE_PIANO){
        int range=par[b+9];int octave=par[layer?P_FMOCT:P_BOCT];
        float gs=(float)par[b+6]*0.01f;
        for(int v=0;v<poly;v++)if(gate[v]){
            int node=s->v_node[v],rank=(int)(s->nodes[node].nx*(float)(range*chord_nt));
            if(rank>=range*chord_nt)rank=range*chord_nt-1;
            if(rank<0)rank=0;
            int pc=chord_pcs[rank%chord_nt],rel=(pc-par[P_ROOT]+12)%12;
            int note=60+par[P_ROOT]+octave*12+rel+(rank/chord_nt)*12;
            while(note<0)note+=12;
            while(note>127)note-=12;
            int vel=58+(s->max_degree?(int)s->nodes[node].degree*45/s->max_degree:0);
            ud_note_count[layer]++;
#ifdef PLAT_HOST
            int ok=0;for(int j=0;j<chord_nt;j++)if(note%12==chord_pcs[j])ok=1;
            if(!ok)ud_chord_fail[layer]++;
#endif
            if(layer){if(par[P_FMLEVEL]>0)fm_seq_schedule(note,vel,0);}
            else {ring_schedule(note,vel+(int)(gs*12.f),v);midi(0x91,(uint32_t)note,(uint32_t)vel);midi(0x81,(uint32_t)note,0);}
        }
    }
    BARRIER();ud_view_version++;
}
static uint32_t ud_gate_samples(void){
    float quarter=en_ext?fm_quarter_samples:step_len*4.f;
    float step=quarter*(float)evolve_rate_divisor(par[P_FUDRATE])*0.25f;
    float g=step*(float)par[P_FUDGATE]*0.01f;
    return (uint32_t)(g<1.f?1.f:g>EN_SR*8.f?EN_SR*8.f:g+0.5f);
}
/* Only inspect graph state for the UI; no DSP runs on the screen thread. */
void en_unitd_read(int layer,en_unitd_screen_t *out){
    const ud_state_t *s=&ud_state[layer];
    uint32_t a,z;
    do {
        a=ud_view_version;BARRIER();
        out->count=s->node_count;out->poly=s->poly;
        for(int j=0;j<UD_POLY;j++)out->active[j]=s->v_node[j];
        for(int j=0;j<UD_NODES;j++){
            out->x[j]=(uint8_t)(ud_clampf(s->nodes[j].nx,0.f,1.f)*255.f);
            out->y[j]=(uint8_t)(ud_clampf(s->nodes[j].ny,0.f,1.f)*255.f);
            out->edges[j]=s->nodes[j].adj;
        }
        BARRIER();z=ud_view_version;
    } while((a&1u)||a!=z);
}
