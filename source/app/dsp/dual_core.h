/* Two-core Rings/FM worker; a fault retains its reason until reboot. */
#pragma once
#include <stdint.h>
enum { DC_FAULT_NONE, DC_FAULT_TIMEOUT, DC_FAULT_MAILBOX, DC_FAULT_STACK };
#if (defined(PLAT_HOST) || defined(EN_HOST)) && !defined(DC_PTHREAD)
/* Host builds exercise the split-buffer DSP path, not hardware startup. */
#if defined(PLAT_HOST)
extern uint32_t host_perf_dual;
static inline int dc_submit(void (*fn)(void *), void *arg) { if (!host_perf_dual) return 0; fn(arg); return 1; }
#else
static inline int dc_submit(void (*fn)(void *), void *arg) { fn(arg); return 1; }
#endif
static inline int dc_wait(void) { return 1; }
static inline unsigned dc_status(void) { return 2; }
static inline unsigned dc_stage(void) { return 3; }
static inline unsigned dc_fault(void) { return DC_FAULT_NONE; }
#else
int dc_submit(void (*fn)(void *), void *arg);
int dc_wait(void);
unsigned dc_status(void);
unsigned dc_stage(void);
unsigned dc_fault(void);
#endif
