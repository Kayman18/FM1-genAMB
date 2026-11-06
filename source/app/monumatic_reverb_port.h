#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void monumatic_reverb_init(void);
void monumatic_reverb_panic(void);
void monumatic_reverb_set(float decay,float tone,float motion);
void monumatic_reverb_process(float *mono,float *right,uint32_t n);
#ifdef __cplusplus
}
#endif
