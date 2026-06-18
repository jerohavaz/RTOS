#include "k_idle.h"
#include "k_panic.h"
#include "k_sched.h"
#include "k_task.h"
#include "os_config.h"
#include "port.h"

void k_idle_create(void) {
    kernel_task_t *idle;
    os_status_t status;

    status = k_task_create_internal(k_idle_task, OS_TASK_PRIORITY_LOWEST, &idle);

    if (status != OS_OK) {
        k_panic();
    }

    k_sched_set_idle_task(idle);
}

void k_idle_task(void) {
    while (1) {
#if OS_TRACE_ENABLED
        /*
         * Do not use WFI while tracing.
         *
         * SystemView reads trace data from the target RAM via J-Link RTT.
         * Entering sleep from WFI can stop or disturb debugger/RTT access,
         * causing missing or stalled SystemView logs.
         *
         * Keep the CPU awake in trace builds; use real idle sleep otherwise.
         */
        port_no_operation();
#else
        port_wait_for_interrupt();
#endif
    }
}