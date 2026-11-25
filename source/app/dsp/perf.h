#pragma once
#include <stdint.h>
typedef struct {
    uint32_t audio_pct, peak_pct, worker_us, wait_us, dropouts, faults, seconds, jobs;
    uint32_t dual, status, cpu0_us, cpu0_peak_us;
    uint32_t worst_total_us, worst_work_us, worst_wait_us;
} perf_stats_t;
void perf_read(perf_stats_t *s);
void perf_reset(void);
void perf_mode(int dual);
