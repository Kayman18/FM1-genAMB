/* Compare shared stereo controls with two independently advanced mono filters. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fm_filter.h"
static uint32_t seed=92421;
static float noise(void) {
    seed=seed*1664525u+1013904223u;
    return (float)(int32_t)seed*(1.0f/2147483648.0f);
}
int main(void) {
    fm_filter_state_t left,right,stereo;
    fm_filter_history_t right_history={0};
    classic_filter_init(&left);classic_filter_init(&right);classic_filter_init(&stereo);
    for(int m=0;m<18;++m) for(int k=0;k<11;++k) {
        fm_filter_state_t *states[]={&left,&right,&stereo};
        for(int i=0;i<3;++i)
            classic_filter_set(states[i],m%6,(m+k)%4,10*k,100-9*k,(k%3)*50,k*.04f,-k*.02f);
        for(int j=0;j<615;++j) {
            float l=noise(),r=noise();
            float a=classic_filter_sample(&left,l), b=classic_filter_sample(&right,r);
            classic_filter_stereo_sample(&stereo,&right_history,&l,&r);
            assert(!memcmp(&a,&l,sizeof a) && !memcmp(&b,&r,sizeof b));
        }
    }
    puts("PASS all six stereo filter models match independent mono channels across modulation and topology changes");
}
