/* SPDX-License-Identifier: ISC */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "../../app/fm_port/fm_filter.c"
static uint32_t seed=912345;
static float noise(void) {seed=seed*1664525u+1013904223u;return (float)(int32_t)seed*(0.20f/2147483648.0f);}
static void state_ok(void) {
    for(int i=0;i<4;i++)assert(isfinite(fs.z[i]) && fabsf(fs.z[i])<1000.0f);
    for(int i=0;i<2;i++)assert(isfinite(fs.sv[i]) && isfinite(fs.high[i]));
}
static double energy(int model,int type,float hz) {
    fm_filter_init();fm_filter_set(model,type,50,0,100,0,0);
    double e=0.0;
    for(int i=0;i<44100;i++){
        float y=fm_filter_sample(0.01f*sinf(6.283185307f*hz*(float)i/44100.0f));
        assert(isfinite(y));if(i>22049)e+=(double)y*y;
    }
    return e;
}
int main(void) {
    unsigned combos=0; float signatures[6]={0};
    for(int model=0;model<6;model++)for(int type=0;type<4;type++)
    for(int cut=0;cut<=100;cut+=20)for(int res=0;res<=100;res+=20){
        fm_filter_init();fm_filter_set(model,type,cut,res,100,0,0);
        for(int i=0;i<8192;i++){
            float x=noise(),y=fm_filter_sample(x);
            assert(isfinite(y) && fabsf(y)<=8.01f);state_ok();
            if(model==0)assert(y==x);
        }
        combos++;
    }
    printf("PASS %u filter model/type/cutoff/resonance combinations, finite output and internal states\n",combos);
    for(int model=1;model<6;model++) {
        fm_filter_init();fm_filter_set(model,0,70,90,100,0,0);
        for(int i=0;i<88200;i++) {
            if(i%256==0)fm_filter_set(model,0,(i/256)%101,90,100,
                sinf((float)i/900.0f),cosf((float)i/1700.0f));
            float y=fm_filter_sample(i<44100?0.5f*noise():0.0f);
            assert(isfinite(y) && fabsf(y)<=8.01f);state_ok();
        }
        fm_filter_set(0,0,70,90,100,0,0);
        for(int i=0;i<256;i++)fm_filter_sample(noise());
        for(int i=0;i<1024;i++){float x=noise();assert(fm_filter_sample(x)==x);}
    }
    puts("PASS modulation sweeps, silence, topology-switch fade and settled exact bypass");
    for(int model=1;model<6;model++) {
        fm_filter_init();fm_filter_set(model,0,65,50,100,0,0);
        for(int i=0;i<4096;i++)fm_filter_sample(noise());
        fm_filter_set(model,0,65,50,0,0,0);
        for(int i=0;i<8192;i++)fm_filter_sample(noise());
        for(int i=0;i<1024;i++){float x=noise();assert(fm_filter_sample(x)==x);}
    }
    puts("PASS zero-mix settled exact dry for all five active models");
    for(int model=1;model<=4;model++) {
        double lo=energy(model,0,50.0f),hi=energy(model,0,6000.0f);
        assert(lo>hi*20.0);
        printf("PASS LP model %d: 50Hz/6kHz power ratio %.2f\n",model,lo/hi);
    }
    for(int model=1;model<=2;model++) {
        double lo=energy(model,1,50.0f),hi=energy(model,1,6000.0f);
        assert(hi>lo*20.0);
        double center=energy(model,2,593.0f),off=energy(model,2,6000.0f);
        assert(center>off*10.0);
        double notch=energy(model,3,593.0f),out=energy(model,3,6000.0f);
        assert(out>notch*100.0);
    }
    puts("PASS clean/SEM HP, BP and notch behavior");
    double low=energy(5,0,10.0f),mid=energy(5,0,300.0f),high=energy(5,0,6000.0f);
    assert(mid>low*20.0 && mid>high*20.0);
    puts("PASS CS-80 linked HP/LP rejects low and high extremes");
    for(int model=1;model<6;model++){
        fm_filter_init();fm_filter_set(model,0,50,60,100,0,0);
        for(int i=0;i<16384;i++){
            float y=fm_filter_sample(0.4f*sinf((float)i*0.015f)+0.03f*sinf((float)i*0.09f));
            if(i>=8192)signatures[model]+=y*y;
        }
    }
    for(int a=1;a<6;a++)for(int b=a+1;b<6;b++)assert(fabsf(signatures[a]-signatures[b])>0.01f);
    puts("PASS all five active models have distinct output on identical input");
    for(int i=0;i<=100;i++){
        float actual=ff_cutoff_g((float)i/100.0f);
        float exact=tanf(3.141592653589793f*20.0f*powf(2.0f,9.78f*(float)i/100.0f)/44100.0f);
        assert(fabsf(actual/exact-1.0f)<0.0015f);
    }
    puts("PASS cutoff coefficient relative error < 0.15% across entire knob range");
    return 0;
}
