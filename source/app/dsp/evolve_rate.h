#ifndef GENAMB_EVOLVE_RATE_H
#define GENAMB_EVOLVE_RATE_H
#include <stdint.h>
/* Five clock divisions for independent Rings and FM Evolve sequencers.
 * Values are sixteenth-note ticks per Evolve step. */
static inline uint32_t evolve_rate_divisor(int rate) {
    switch (rate) {case -1: return 16u; case 0: return 8u; case 1: return 4u; case 2: return 2u; case 3: return 1u; default: return 4u;}
}
/* Old IDs -1..3 retain their exact meaning in saved format-22 projects.
 * New choices are appended IDs; UI steps through musical order explicitly. */
#define SEQ_RATE_COUNT 13
static inline int seq_rate_from_index(int index) {
    static const int8_t ids[13]={-1,4,5,6,0,7,1,8,2,9,10,11,3};
    return ids[index<0?0:index>12?12:index];
}
static inline int seq_rate_index(int rate) {
    for(int i=0;i<13;i++)if(seq_rate_from_index(i)==rate)return i;
    return 6;
}
static inline const char *seq_rate_label(int rate) {
    static const char *const labels[13]={"1/4x","1/3.5x","1/3x","1/2.5x","1/2x","1/1.5x","1x","1.5x","2x","2.5x","3x","3.5x","4x"};
    return labels[seq_rate_index(rate)];
}
/* Rate is n/d steps per quarter. Find the next event strictly within this
 * sixteenth. Fractional offsets are rounded to the nearest audio sample.
 * No per-sample clocks or cumulative floating-point phase error. */
static inline int32_t seq_rate_offset(int rate,uint32_t grid,float sixteenth_samples) {
    static const uint8_t numer[13]={1,2,1,2,1,2,1,3,2,5,3,7,4};
    static const uint8_t denom[13]={4,7,3,5,2,3,1,2,1,2,1,2,1};
    int i=seq_rate_index(rate);uint32_t n=numer[i],period=4u*denom[i];
    uint32_t phase=((grid%period)*n)%period;
    if(!phase)return 0;
    uint32_t distance=period-phase;
    if(distance>=n)return -1;
    return (int32_t)(sixteenth_samples*(float)distance/(float)n+0.5f);
}
#endif
