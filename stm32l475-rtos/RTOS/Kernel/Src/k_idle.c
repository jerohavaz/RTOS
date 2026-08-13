/**
 * @file k_idle.c
 * @brief Kernel idle-task implementation.
 * @author Jerome
 */

#include "k_idle.h"
#include "kernel_panic.h"
#include "k_sched.h"
#include "k_task.h"
#include "os_config.h"
#include "port.h"

void k_idle_create(void) {
    kernel_task_t *idle;

    os_status_t status = k_task_create_internal(k_idle_task, OS_TASK_PRIORITY_LOWEST, true, &idle);

    KERNEL_REQUIRE(status == OS_OK);
    KERNEL_REQUIRE(idle != 0);

    k_sched_set_idle_task(idle);
}

void k_idle_task(void) {
    while (1) {
#if OS_TRACE_ENABLED
        /*
         * Keep the CPU awake while tracing so RTT-based trace transport and
         * debugger access remain responsive. Non-trace builds may sleep until
         * the next interrupt.
         */
        port_no_operation();
#else
        port_wait_for_interrupt();
#endif
    }
}
