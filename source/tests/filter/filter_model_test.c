/* SPDX-License-Identifier: ISC
 * genAMB v20: exercise selected post-FM model DSP directly at extremes.
 * No other synth engine or GUI dependencies, with ASan/UBSan enabled.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "../../app/fm_port/fm_filter.c"
static float sequence[1024];
int main(void) {
    unsigned n=0,changed=0;uint32_t seed=912345;
    for(int i=0;i<1024;i++) {seed=seed*1664525u+1013904223u;sequence[i]=(float)(int32_t)seed*(1.0f/2147483648.0f)*0.20f;}
    fm_filter_init();
    fm_filter_set(0,3,78,25,100,0,0);
    for(int i=0;i<1024;i++)assert(fm_filter_sample(sequence[i])==sequence[i]);
    for(int model=0;model<=6;model++)for(int type=0;type<4;type++)
        for(int cutoff=0;cutoff<=100;cutoff+=25)for(int res=0;res<=100;res+=25){
            fm_filter_init();fm_filter_set(model,type,cutoff,res,100,0,0);
            float tot=0,max=0;
            for(int i=0;i<1024;i++){
                float out=fm_filter_sample(sequence[i]);
                assert(isfinite(out)); assert(fabsf(out)<4.0f);
                tot+=fabsf(out);max=fmaxf(max,fabsf(out));
                if(fabsf(out-sequence[i])>1e-6f)changed++;
            }
            assert(tot>=0&&max<4);
            n++;
        }
    /* Mid-play changes, hard resonance, and changing cutoff under both modulators. */
    for(int model=0;model<7;model++)for(int i=0;i<8192;i++){
        fm_filter_set(model,i%4,i%101,i%101,75,(i%9-4)/4.0f,(i%7-3)/3.0f);
        float out=fm_filter_sample(sequence[i%1024]);
        assert(isfinite(out) && fabsf(out)<4.0f);
    }
    fm_filter_set(4,0,45,60,0,0,0);
    for(int i=0;i<1024;i++)assert(fm_filter_sample(sequence[i])==sequence[i]);
    printf("PASS: %u model/type/cut/res combinations, %u non-bypass samples, parameter-sweep finite/stable, 0%% wet and Standard bypass bit identical\n",n,changed);
    assert(changed>10000);
    return 0;
}
