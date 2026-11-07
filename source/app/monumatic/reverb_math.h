#pragma once
#define MM_FM1_FLOAT_ONLY 1
/* FM-1 arithmetic adaptation, also used by the host listening-test build.
 * The AC79 has float multiply/add but no fused multiply-add. Its SDK fmaf
 * pulls in software double arithmetic; use separate float operations here.
 * Original ARM comparison tests compile src/ directly and retain true fmaf.
 * These bounded fastmath functions come from the existing FM-1 platform.
 * Consequently this listening-test firmware is not bit-exact to ARM. */
#include <math.h>
#include <string.h>
#include "fastmath.h"
/* The platform is freestanding (-fno-builtin), but fixed-size state copies
 * in the DSP must be compiler intrinsics instead of byte-loop libc calls. */
#define memcpy __builtin_memcpy
#define memset __builtin_memset
static inline __attribute__((always_inline)) int mm_target_finite(float x){fm_bits_t b;b.f=x;return (b.u&0x7f800000u)!=0x7f800000u;}
#undef isfinite
#define isfinite mm_target_finite
static inline __attribute__((always_inline)) float mm_target_madd(float a,float b,float c){return a*b+c;}
#define fmaf mm_target_madd
#define sinf fm_sinf
#define tanf fm_tanf
#define fabsf fm_fabsf
#define fminf fm_minf
#define fmaxf fm_maxf
#define powf fm_powf
#define expf fm_expf
#define logf fm_logf
