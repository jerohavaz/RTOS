#include "kernel_task.h"
#include "os.h"
#include "k_timeout.h"
#include "k_sched.h"
#include "os_types.h"
#include "port.h"
#include <stdint.h>

os_status_t os_delay(uint32_t delay_ticks) {
    if (port_in_exception() != 0u) {
        return OS_ERR_INVALID_STATE;
    }

    if (delay_ticks == 0u) {
        k_sched_request_yield();
        return OS_OK;
    }

    kernel_task_t *task = k_sched_current();

    if (task == 0) {
        return OS_ERR_INVALID_STATE;
    }

    if (k_sched_is_idle(task) != 0u) {
        return OS_ERR_INVALID_STATE;
    }

    uint32_t key = port_enter_critical();

    task->wait_object = 0;
    task->wait_type = K_WAIT_DELAY;
    task->wait_result = OS_OK;

    k_timeout_add(task, delay_ticks);
    k_sched_task_block(task);

    k_sched_request_switch();
    port_exit_critical(key);

    return task->wait_result;
}