#include "os_isr.h"
#include "k_sched.h"
#include "k_timeout.h"
#include "trace.h"

void os_isr_enter(void) {
    trace_isr_enter();
}

void os_systick_tick(void) {
    if (k_sched_started() == 0u) {
        return;
    }

    k_tick_inc();
    trace_tick(1);

    k_timeout_process_tick();
}

void os_isr_exit(void) {
    if (k_sched_started() == 0u) {
        trace_isr_exit();
        return;
    }

    if (k_sched_request_yield() != 0u) {
        trace_isr_exit_to_scheduler();
        return;
    }

    trace_isr_exit();
    return;
}