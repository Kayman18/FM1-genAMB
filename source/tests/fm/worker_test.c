/* SPDX-License-Identifier: MPL-2.0
 * Compile the production worker, with only its hardware boundary stubbed.
 * Virtual time makes watchdog boundaries and timer wrap deterministic. */
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_BLOCK 256u
#include "../../base/device/dual_core.c"

/* GNU-compatible assembler aliases model the actual linker's three positions
 * within ONE arena. Separate C arrays would make the guard traversal undefined
 * and would depend on the compiler's ordering of globals. */
uint32_t fm_test_stack_arena[64u + 2048u + 128u];
__asm__(".globl _cpu1_stack_guard\n"
        ".set _cpu1_stack_guard, fm_test_stack_arena\n"
        ".globl _cpu1_stack_low\n"
        ".set _cpu1_stack_low, fm_test_stack_arena + 256\n"
        ".globl _cpu1_sstack_top\n"
        ".set _cpu1_sstack_top, fm_test_stack_arena + 8960\n");

static uint32_t fake_tick, completion_after, completion_pending;
static unsigned held, holds, calls, checks, worker_escape_mode;
static unsigned worker_value; /* survives the longjmp used to exit CPU1's loop */
static jmp_buf worker_escape;

#define CHECK(label, condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, label); \
        abort(); \
    } \
    ++checks; \
} while (0)

uint32_t fm1_ticks(void)
{
    uint32_t now = fake_tick;
    fake_tick += FM1_TICKS_PER_US; /* one virtual microsecond per clock read */
    if (!held && completion_pending &&
        (uint32_t)(now - dc_start_tick) >= completion_after) {
        completion_pending = 0;
        dc_in.function(dc_in.argument);
        dc_out.ticks = completion_after;
        dc_out.done = dc_in.request;
    }
    return now;
}
void fm1_cpu1_sync(void)
{
    /* Exercise the actual CPU1 validation/dispatch loop without host threads.
     * Return to the test only once it parks after completion or reports bad. */
    if (worker_escape_mode && dc_out.bad)
        longjmp(worker_escape, 1);
    if (worker_escape_mode && calls && dc_out.done == dc_in.request)
        longjmp(worker_escape, 1);
}
void fm1_cpu1_hold(void) { held = 1; ++holds; }
uint32_t fm1_cpu1_launch(void) { held = 0; dc_out.ready = 1; return 7; }
void fm1_cpu1_restore_div(uint32_t div) { assert(div == 7); }
void fm1_irq_off(void) {}
void fm1_cpu1_stack_guard(uint32_t low, uint32_t high)
{
    assert(low == (uint32_t)(uintptr_t)_cpu1_stack_low);
    assert(high == (uint32_t)(uintptr_t)_cpu1_sstack_top - 1u);
}
void fm1_guard_unlock_top(void) {}
void fm1_guard_cpu1_boot_open(uint32_t saved[4])
{
    for (unsigned i = 0; i < 4; ++i) saved[i] = 0xB001u + i;
}
void fm1_guard_cpu1_boot_close(const uint32_t saved[4])
{
    for (unsigned i = 0; i < 4; ++i) assert(saved[i] == 0xB001u + i);
}

static void job(void *arg)
{
    ++*(unsigned *)arg;
    ++calls;
}
static void reset_worker(uint32_t initial_tick)
{
    fake_tick = initial_tick;
    completion_after = completion_pending = worker_escape_mode = 0;
    held = holds = calls = 0;
    dc_enabled = 1;
    dc_jobs = dc_timeouts = dc_max_wait_us = 0;
    dc_block_work_us = dc_block_wait_us = 0;
    dc_boot();
    CHECK("boot enables a ready worker", dc_status() == 2 && dc_fault() == DC_FAULT_NONE);
    CHECK("boot fills every guard word", fm_test_stack_arena[0] == 0xC1A5CAFEu &&
          fm_test_stack_arena[63] == 0xC1A5CAFEu);
    holds = 0;
}
static void completes_after(uint32_t us)
{
    completion_after = us * FM1_TICKS_PER_US;
    completion_pending = 1;
}
static void check_sticky(unsigned reason, unsigned expected_calls)
{
    unsigned value = 0;
    uint32_t request = dc_in.request;
    CHECK("fault retains distinct cause", dc_status() == 3 && dc_fault() == reason);
    CHECK("fault holds CPU1 exactly once", held && holds == 1);
    CHECK("fault rejects future submissions", !dc_submit(job, &value));
    CHECK("fault never reruns a partial job", dc_in.request == request && calls == expected_calls && !value);
    dc_enabled = 0;
    CHECK("single-mode selection cannot hide an unsafe stopped worker", dc_status() == 3);
}

static void test_real_worker_dispatch(void)
{
    worker_value = 0;
    reset_worker(0);
    CHECK("normal job submitted", dc_submit(job, &worker_value));
    worker_escape_mode = 1;
    if (!setjmp(worker_escape)) fm1_cpu1_main();
    worker_escape_mode = 0;
    CHECK("real CPU1 loop calls the job once", worker_value == 1 && calls == 1);
    CHECK("real CPU1 loop publishes completion", dc_out.done == dc_in.request && !dc_out.bad);
    CHECK("completed job joins successfully", dc_wait() && dc_jobs == 1 && !dc_timeouts);
    CHECK("normal completion preserves dual mode", dc_status() == 2 && dc_fault() == DC_FAULT_NONE);
}
static void test_completion(uint32_t us, uint32_t initial_tick)
{
    unsigned value = 0;
    reset_worker(initial_tick);
    CHECK("timed job submitted", dc_submit(job, &value));
    completes_after(us);
    CHECK("timed completion does not fault", dc_wait());
    CHECK("timed job runs exactly once", value == 1 && calls == 1 && dc_jobs == 1);
    CHECK("timing stats count actual worker duration", dc_block_work_us == us && dc_max_wait_us >= us - 1u);
    CHECK("timed completion keeps worker available", dc_status() == 2 && dc_fault() == DC_FAULT_NONE && !holds);
    if (initial_tick > UINT32_MAX / 2u)
        CHECK("this test actually crosses timer wrap", fake_tick < initial_tick);
}
static void test_timeout(uint32_t initial_tick)
{
    unsigned value = 0;
    reset_worker(initial_tick);
    CHECK("hanging job submitted", dc_submit(job, &value));
    completes_after(DC_HANG_US + 100u);
    CHECK("unfinished job faults beyond watchdog", !dc_wait());
    uint32_t elapsed = (fake_tick - dc_start_tick) / FM1_TICKS_PER_US;
    CHECK("hang remains bounded at the new deadline", elapsed > DC_HANG_US && elapsed <= DC_HANG_US + 2u);
    CHECK("timeout records no completed job", dc_timeouts == 1 && dc_jobs == 0 && value == 0);
    check_sticky(DC_FAULT_TIMEOUT, 0);
    if (initial_tick > UINT32_MAX / 2u)
        CHECK("timeout test crosses timer wrap", fake_tick < initial_tick);
    dc_enabled = 1;
    completion_pending = 0;
    dc_boot();
    CHECK("reboot clears retained fault", dc_status() == 2 && dc_fault() == DC_FAULT_NONE);
}
static void test_mailbox(unsigned corruption)
{
    worker_value = 0;
    reset_worker(0);
    CHECK("mailbox test job submitted", dc_submit(job, &worker_value));
    if (corruption == 0) dc_in.check ^= 1u;
    if (corruption == 1) dc_in.request += 1u;
    if (corruption == 2) dc_in.function = 0;
    worker_escape_mode = 2;
    if (!setjmp(worker_escape)) fm1_cpu1_main();
    worker_escape_mode = 0;
    CHECK("actual worker rejects corrupt mailbox before dispatch", dc_out.bad && !calls && !worker_value);
    CHECK("mailbox failure faults the join", !dc_wait());
    check_sticky(DC_FAULT_MAILBOX, 0);
}
static void test_stack(void)
{
    unsigned value = 0;
    reset_worker(0);
    CHECK("stack test job submitted", dc_submit(job, &value));
    completes_after(20);
    fm_test_stack_arena[63] ^= 1u;
    CHECK("completed job with damaged stack guard faults", !dc_wait());
    CHECK("stack fault excludes the completed job from successful stats", value == 1 && dc_jobs == 0 && dc_timeouts == 1);
    check_sticky(DC_FAULT_STACK, 1);
}

int main(void)
{
    CHECK("watchdog uses two rounded-up audio halves", DC_AUDIO_HALF_US == 5805u && DC_HANG_US == 11610u);
    test_real_worker_dispatch();
    test_completion(50, 0);
    test_completion(4500, 0);  /* regression: old 4 ms watchdog muted both layers */
    test_completion(DC_HANG_US, 0);
    test_completion(4500, UINT32_MAX - 2000u * FM1_TICKS_PER_US);
    test_timeout(0);
    test_timeout(UINT32_MAX - 2000u * FM1_TICKS_PER_US);
    for (unsigned i = 0; i < 3; ++i) test_mailbox(i);
    test_stack();
    printf("worker policy: %u checks passed (normal/4.5 ms/boundary/hang/mailbox/stack/timer wrap)\n", checks);
    return 0;
}
