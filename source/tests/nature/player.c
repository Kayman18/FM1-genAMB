/* Tests the same block decoder and renderer that run on the FM-1. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fastmath.h"
#define EN_POOL
#include "nature.inc"

static float l[256], r[256];

static void render(unsigned n)
{
    memset(l,0,sizeof l); memset(r,0,sizeof r);
    nature_render(l,r,n);
}
static void enable(unsigned water,unsigned rain,unsigned animals,unsigned density)
{
    nature_reset(0x4e415455u ^ 0x9e3779b9u,0x4e415455u ^ 0x85ebca6bu);
    nature_control(0,100); nature_control(1,water); nature_control(2,rain);
    nature_control(3,animals); nature_control(4,density);
}
static uint64_t mix_hash(int variable)
{
    uint64_t hash=1469598103934665603ull;
    unsigned seq=1;
    enable(85,100,100,100);
    for (unsigned part=0;part<5;++part) {
        if (part==1) nature_control(1,37);
        if (part==2) nature_control(4,0);
        if (part==3) nature_control(0,0);
        if (part==4) nature_control(0,60);
        for(unsigned todo=44100u*11u;todo;) {
            unsigned n=variable ? 1u+((seq++*137u)%256u) : 256u;
            if(n>todo)n=todo;
            render(n);
            for(unsigned i=0;i<n;++i) {
                uint32_t a,b;
                memcpy(&a,&l[i],4); memcpy(&b,&r[i],4);
                assert(isfinite(l[i]) && isfinite(r[i]));
                hash=(hash^a)*1099511628211ull;
                hash=(hash^b)*1099511628211ull;
            }
            todo-=n;
        }
    }
    return hash;
}
static void test_water(void)
{
    enable(100,0,0,0);
    double energy=0;
    unsigned span=0, stereo=0;
    for(unsigned b=0;b<6400;++b){
        render(256);
        for(unsigned i=0;i<256;++i) {
            energy+=l[i]*l[i]; stereo += l[i]!=r[i];
            if (++span==11025) { assert(energy>0.0001); span=0; energy=0; }
        }
    }
    assert(stereo>1000000);
    nature_control(1,0);
    for(unsigned i=0;i<2204;i+=4)render(4);
    render(1); assert(l[0]==0 && r[0]==0 && !nature.water[0].asset);
}
static void test_rain(void)
{
    const unsigned lo[]={0,45,90,60,90}, hi[]={0,90,180,120,240};
    enable(0,100,0,0);
    render(1);
    for(unsigned c=0;c<8;++c){
        unsigned phase=c%4+1;
        assert(nature.phase==phase);
        assert(nature.phase_length>=lo[phase]*44100 && nature.phase_length<=hi[phase]*44100);
        unsigned todo=nature.phase_length-nature.phase_pos;
        double energy=0;
        while(todo){
            unsigned n=todo>256?256:todo;
            render(n);
            for(unsigned i=0;i<n;++i)energy+=l[i]*l[i];
            todo-=n;
        }
        if(phase==NATURE_DRY) assert(energy==0);
        else assert(energy>1);
    }
}
static unsigned bird_run(unsigned density)
{
    unsigned count=0, seen=0, overlaps=0;
    enable(0,0,100,density);
    for(unsigned block=0;block<(600u*44100u)/128u;++block){
        const NatureAsset *old[2]={nature.bird[0].asset,nature.bird[1].asset};
        unsigned left[2]={nature.bird[0].remaining,nature.bird[1].remaining};
        render(128);
        if(NATURE_BIRDS(nature_status)==2)++overlaps;
        for(unsigned b=0;b<2;b++) {
            const NatureAsset *v=nature.bird[b].asset;
            if(old[b] && left[b]>128) assert(v==old[b] && nature.bird[b].remaining==left[b]-128);
            if(v && v!=old[b]){
                unsigned k=0;
                while(k<animal_count && nature_animal_asset(k)!=v)++k;
                assert(k<animal_count);
                ++count; seen|=1u<<k;
            }
        }
    }
    assert(seen==((1u<<animal_count)-1u) && count>=animal_count);
    if(density==100)assert(overlaps>0);
    /* Switch density off during a call; its remaining frames must still play. */
    while (!nature.bird[0].asset && !nature.bird[1].asset)render(128);
    unsigned b=nature.bird[0].asset?0:1, remain=nature.bird[b].remaining;
    nature_control(4,0);
    while(remain>1){unsigned n=remain>256?256:remain-1;render(n);remain-=n;}
    assert(nature.bird[b].asset); render(1); assert(!nature.bird[b].asset);
    for (unsigned i=0;i<2000;i++) render(256);
    assert(NATURE_BIRDS(nature_status)==0);
    return count;
}
static uint8_t external_wave[ANIMAL_WAVE_END-ANIMAL_WAVE_BASE];

static void verify_bank(void)
{
    FILE *f=fopen("assets/nature/animal-wave.bin","rb");
    assert(f && fread(external_wave,1,sizeof external_wave,f)==sizeof external_wave);
    assert(fgetc(f)==EOF); fclose(f);
    en_nature_mount(NULL,0); assert(en_nature_calls()==7);
    en_nature_mount(external_wave,63); assert(en_nature_calls()==7);
    external_wave[31]^=1;
    en_nature_mount(external_wave,sizeof external_wave); assert(en_nature_calls()==7);
    external_wave[31]^=1;
    external_wave[300]^=1;
    en_nature_mount(external_wave,sizeof external_wave); assert(en_nature_calls()==7);
    external_wave[300]^=1;
    en_nature_mount(external_wave,sizeof external_wave); assert(en_nature_calls()==10);
    puts("PASS three full external calls mounted; absent, truncated and corrupt banks fall back to seven");
}

static void verify_pan(void)
{
    unsigned mask=0,up=0,down=0;
    enable(0,0,100,0);
    for (unsigned k=0;k<animal_count;++k) {
        nature_start_bird();
        NatureVoice *v=&nature.bird[0];
        unsigned id=nature.last_bird;
        assert(!(mask&(1u<<id))); mask|=1u<<id;
        assert(v->asset==nature_animal_asset(id));
        float start=v->right*v->right;
        unsigned total=v->remaining;
        assert(total==(v->asset->frames*441u+159u)/160u);
        for(unsigned frame=0;frame<total;frame++) {
            if (frame==1024) {
                /* Inject a reply and compare each gain with its own trajectory. */
                NatureVoice saved=*v;
                unsigned rng=nature.bird_rng;
                uint8_t bag[MAX_CALLS]; memcpy(bag,nature.bag,sizeof bag);
                uint8_t last=nature.last_bird,idx=nature.bag_index;
                nature_start_bird();
                assert(nature.bird[1].asset && !memcmp(&saved,v,sizeof saved));
                NatureVoice *reply=&nature.bird[1];
                float beginning=reply->right, original_right=v->right;
                for (int n=0;n<256;++n) {
                    nature_advance_pan(reply);nature_sample(reply,0);
                    nature_advance_pan(v);nature_sample(v,0);
                }
                assert(reply->right!=beginning && v->right!=original_right);
                *v=saved; reply->asset=NULL;
                nature.bird_rng=rng; nature.last_bird=last; nature.bag_index=idx;
                memcpy(nature.bag,bag,sizeof bag);
            }
            nature_advance_pan(v);
            float power=v->left*v->left+v->right*v->right;
            assert(fabsf(power-1.0f)<0.0001f);
            assert(fabsf(v->right*v->right-(start+(v->pan_end-start)*(float)(frame+1)/total))<0.0001f);
            nature_sample(v,0);
        }
        assert(!v->asset && fabsf(v->right*v->right-v->pan_end)<0.0001f);
        if (start<v->pan_end) ++up; else ++down;
        assert((start-.5f)*(v->pan_end-.5f)<0);
    }
    assert(mask==1023 && up && down);
    puts("PASS ten complete calls sweep across center, both directions, independent two-voice panning, near-unity pan power");
}

int main(void)
{
    nature_init_decoder();
    verify_bank(); verify_pan();
    test_water(); test_rain();
    unsigned low=bird_run(1), mid=bird_run(35), high=bird_run(100);
    assert(low<mid && mid<high);
    uint64_t h=mix_hash(0); assert(h==mix_hash(1));
    printf("PASS water loops, 50 ms mute, two rain cycles, ten complete calls, two-voice overlap, density zero and chunk independence\n");
    printf("Calls in ten minutes at 1/35/100: %u/%u/%u. Reconstruction render hash: %016llx\n",
        low,mid,high,(unsigned long long)h);
}
