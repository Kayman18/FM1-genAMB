/* SPDX-License-Identifier: MPL-2.0
 * Deterministic hardware boundary for the real device worker policy test. */
#pragma once
#include <stdint.h>
#define FM1_TICKS_PER_US 24u
uint32_t fm1_ticks(void);
void fm1_cpu1_sync(void);
void fm1_cpu1_hold(void);
uint32_t fm1_cpu1_launch(void);
void fm1_cpu1_restore_div(uint32_t div);
void fm1_irq_off(void);
void fm1_cpu1_stack_guard(uint32_t low, uint32_t high);
void fm1_guard_unlock_top(void);
void fm1_guard_cpu1_boot_open(uint32_t saved[4]);
void fm1_guard_cpu1_boot_close(const uint32_t saved[4]);
