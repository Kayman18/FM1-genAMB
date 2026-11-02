/* Experimental polled CPU1 worker. Only CPU0 submits; one job at a time.
 * Completion is published inside a RAM-resident idle loop so flash writes on
 * CPU0 cannot race CPU1 fetching idle instructions from XIP. */
#include "dual_core.h"
#include "fm1_cpu1.h"
/* Each core publishes on a separate 64-byte cache line. */
static struct {
    volatile uint32_t request, check;
    void (*volatile function)(void *);
    void *volatile argument;
    uint32_t padding[12];
} dc_in __attribute__((aligned(64)));
static struct {
    volatile uint32_t done, ready, bad, ticks;
    uint32_t padding[12];
} dc_out __attribute__((aligned(64)));
static volatile uint32_t dc_mode;
static volatile uint32_t dc_fault_code;
static uint32_t dc_enabled = 1, dc_block_work_us, dc_block_wait_us;
volatile uint32_t dc_trace;
#define DC_CHECK(f, a, n) ((uint32_t)(uintptr_t)(f) ^ (uint32_t)(uintptr_t)(a) ^ ((n) * 0x9E3779B1u))
static uint32_t dc_start_tick;
/* Audio overruns are measured by render_block(). The worker hang watchdog is
 * separate: one transient slow block must not permanently mute both voices.
 * Bound it to two audio halves (11.61 ms at 256/44.1 kHz), retaining all
 * mailbox and stack guards. The old 4 ms limit was shorter than one half. */
#define DC_AUDIO_HALF_US ((APP_BLOCK * 1000000u + 44099u) / 44100u)
#define DC_HANG_US (2u * DC_AUDIO_HALF_US)
extern uint32_t _cpu1_stack_guard[], _cpu1_stack_low[], _cpu1_sstack_top[];
volatile uint32_t dc_jobs, dc_timeouts, dc_max_wait_us;
/* Linker reserves a guarded 8 KiB CPU1 stack in unused retained RAM. */
static __attribute__((section(".ram_text"),noinline,used)) uint32_t dc_park(uint32_t completed)
{
    fm1_cpu1_sync();
    dc_out.done = completed;
    dc_trace = 3;
    dc_out.ready = 1;
    fm1_cpu1_sync();
    while (dc_in.request == completed) fm1_cpu1_sync();
    fm1_cpu1_sync();
    return dc_in.request;
}
void fm1_cpu1_main(void)
{
    uint32_t seq = 0;
    fm1_irq_off();
    dc_trace = 2;
    fm1_cpu1_stack_guard((uint32_t)(uintptr_t)_cpu1_stack_low, (uint32_t)(uintptr_t)_cpu1_sstack_top - 1u);
    for (;;) {
        uint32_t (*volatile park)(uint32_t) = dc_park;
        uint32_t next = park(seq);
        void (*fn)(void *) = dc_in.function;
        void *arg = dc_in.argument;
        fm1_cpu1_sync();
        if (next != seq + 1u || !fn || dc_in.check != DC_CHECK(fn, arg, next)) {
            dc_out.bad = 1;
            /* CPU0 will hold the core; no work is attempted with bad pointers. */
            for (;;) fm1_cpu1_sync();
        }
        uint32_t start = fm1_ticks();
        fn(arg);
        dc_out.ticks = fm1_ticks() - start;
        seq = next;
    }
}
static void dc_stop(void)
{
    fm1_cpu1_hold();
}
static void dc_boot(void)
{
    uint32_t div, start, pc_limits[4];
    dc_stop();
    for (uint32_t *p = _cpu1_stack_guard; p < _cpu1_stack_low; ++p) *p = 0xC1A5CAFEu;
    dc_trace = dc_out.bad = 0;
    dc_fault_code = DC_FAULT_NONE;
    dc_in.request = dc_out.done = dc_out.ready = 0;
    dc_mode = 1; /* startup fallback: normal single-core audio */
    fm1_guard_unlock_top();
    fm1_guard_cpu1_boot_open(pc_limits);
    div = fm1_cpu1_launch();
    start = fm1_ticks();
    while (!dc_out.ready && (uint32_t)(fm1_ticks() - start) < 100000u * FM1_TICKS_PER_US) { fm1_cpu1_sync(); }
    fm1_cpu1_restore_div(div);
    if (dc_out.ready) dc_mode = 2;
    else dc_stop();
    fm1_guard_cpu1_boot_close(pc_limits);
}
unsigned dc_status(void) { return dc_mode == 2 && !dc_enabled ? 1 : dc_mode; }
unsigned dc_stage(void) { return dc_trace; }
unsigned dc_fault(void) { return dc_fault_code; }
int dc_submit(void (*fn)(void *), void *arg)
{
    if (dc_mode != 2 || !dc_enabled) return 0;
    dc_in.function = fn; dc_in.argument = arg;
    dc_in.check = DC_CHECK(fn, arg, dc_in.request + 1u);
    dc_start_tick = fm1_ticks();
    fm1_cpu1_sync();
    ++dc_in.request;
    fm1_cpu1_sync();
    return 1;
}
int dc_wait(void)
{
    uint32_t t = fm1_ticks();
    while (dc_out.done != dc_in.request) {
        if (dc_out.bad || (uint32_t)(fm1_ticks() - dc_start_tick) > DC_HANG_US * FM1_TICKS_PER_US) {
            uint32_t reason = dc_out.bad ? DC_FAULT_MAILBOX : DC_FAULT_TIMEOUT;
            dc_stop(); dc_fault_code = reason; dc_mode = 3; ++dc_timeouts;
            return 0; /* never rerun a partially rendered Rings block */
        }
    }
    fm1_cpu1_sync();
    t = (fm1_ticks() - t) / FM1_TICKS_PER_US;
    if (t > dc_max_wait_us) dc_max_wait_us = t;
    for (uint32_t *p = _cpu1_stack_guard; p < _cpu1_stack_low; ++p)
        if (*p != 0xC1A5CAFEu) { dc_stop(); dc_fault_code = DC_FAULT_STACK; dc_mode = 3; ++dc_timeouts; return 0; }
    dc_block_work_us += dc_out.ticks / FM1_TICKS_PER_US;
    dc_block_wait_us += t;
    ++dc_jobs;
    return 1;
}
