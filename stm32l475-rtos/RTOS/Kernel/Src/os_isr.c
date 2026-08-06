/**
 * @file os_isr.c
 * @brief RTOS interrupt lifecycle and system-tick implementation.
 * @author Jerome
 */

#include "os_isr.h"
#include "k_sched.h"
#include "k_timeout.h"
#include "trace.h"

void os_isr_enter(void) {
    trace_isr_enter();
}

void os_systick_tick(void) {
    if (!k_sched_started()) {
        return;
    }

    k_tick_inc();
    trace_tick(1);

    k_timeout_process_tick();
}

void os_isr_exit(void) {
    if (!k_sched_started()) {
        trace_isr_exit();
        return;
    }

    if (k_sched_request_yield()) {
        /*
         * The ISR normally returns toward task context. With PendSV pending,
         * Cortex-M tail-chains into the context-switch handler before Thread
         * mode resumes, so account the interval after ISR exit as scheduler
         * time.
         */
        trace_isr_exit_to_scheduler();
        return;
    }

    trace_isr_exit();
    return;
}
