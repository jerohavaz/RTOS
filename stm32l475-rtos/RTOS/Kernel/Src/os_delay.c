#include "k_delay.h"
#include "k_sched.h"
#include "k_timeout.h"
#include "kernel_panic.h"
#include "port.h"
#include "trace.h"
#include "os_delay.h"

os_status_t os_delay(uint32_t delay_ticks) {
    if (port_in_exception()) {
        return OS_ERR_IN_ISR;
    }

    if (delay_ticks == 0u) {
        k_sched_request_yield();
        return OS_OK;
    }

    /*
     * timeout_list ordering uses signed tick subtraction, so delays must stay
     * below 2^31 ticks.
     */
    if (delay_ticks >= K_TIMEOUT_MAX) {
        return OS_ERR_INVALID_ARG;
    }

    kernel_task_t *current = k_sched_current();

    if (current == 0 || k_sched_is_idle(current)) {
        return OS_ERR_INVALID_STATE;
    }

    uint32_t key = port_enter_critical();

    current->wait_object = 0;
    current->wait_type = K_WAIT_DELAY;
    current->wait_result = OS_ERR_BUSY;

    k_timeout_add(current, delay_ticks);
    k_sched_task_block(current);
    port_exit_critical(key);

    k_sched_request_switch();

    return current->wait_result;
}

os_status_t os_delay_busy(uint32_t delay_ticks) {
    
    if (delay_ticks == 0u || delay_ticks >= 0x80000000u) {
        return OS_ERR_INVALID_ARG;
    }

    kernel_task_t *task = k_sched_current();
    if(task == 0){
        return OS_ERR_INVALID_STATE;
    }

    uint32_t start_tick = k_tick_get();
    uint32_t target_ticks = delay_ticks;

    trace_task_delay_busy_start(&task->tcb, delay_ticks);

    while ((int32_t)(k_tick_get() - start_tick) < (int32_t) target_ticks) {
        port_no_operation();
    }

    trace_task_delay_busy_end(&task->tcb);

    return OS_OK;
}

void k_delay_timeout_cleanup(kernel_task_t *task) {
    KERNEL_REQUIRE(task != 0);
    KERNEL_REQUIRE(task->wait_type == K_WAIT_DELAY);
    KERNEL_REQUIRE(task->wait_object == 0);

    task->wait_type = K_WAIT_NONE;
    task->wait_object = 0;
    task->wait_result = OS_OK;
}