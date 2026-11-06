#ifndef MM_REVERB_H
#define MM_REVERB_H
#include <stddef.h>
#include <stdint.h>
/* Recovered kernel; connected through the FM-1 reverb_fx routing wrapper.
 * Source header layout retained; final line word is a compact arena index,
 * replacing the source's absolute 32-bit buffer pointer. */
typedef struct {
 int32_t write,length,read,fraction,step,reserved,seed,count;
 float filtered;
 uint32_t offset;
} MM_ReverbLine;
typedef struct {
 float feedback,cutoff,sample_rate,pitch_mod,previous_frequency,live_rate,damping,previous_cutoff;
 int32_t initialized;
 MM_ReverbLine line[8];
} MM_Reverb;
size_t mm_reverb_samples(float sample_rate);
/* Caller owns a compact float arena. 8..48 kHz validated; return 1 on success.
 * Insufficient capacity/invalid arguments leave existing state/arena unchanged. */
int mm_reverb_initialize(MM_Reverb *s,float sample_rate,float *arena,size_t samples);
/* Reference arithmetic by default; isolated reverb_target_math.h selects the
 * float-only adaptation. Caller must retain valid initialized metadata. */
int mm_reverb_process(MM_Reverb *s,float *arena,float left,float right,float out[2]);
#endif
