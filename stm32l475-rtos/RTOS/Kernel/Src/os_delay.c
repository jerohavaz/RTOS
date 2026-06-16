#include "os.h"
#include "k_timeout.h"
#include "k_sched.h"
#include "os_types.h"
#include "port.h"
#include <stdint.h>

os_status_t os_delay(uint32_t delay_ticks) {
    kernel_task_t *task;

    if (delay_ticks == 0u) {
        k_sched_request_switch();
        return OS_OK;
    }

    task = k_sched_current();

    if (task == 0) {
        return OS_ERR_INVALID_STATE;
    }

    uint32_t irq = port_enter_critical();

    // TODO: dont delay idle
    task->wait_object = 0;
    task->wait_result = OS_OK;
    k_sched_task_block(task);

    k_timeout_add(task, delay_ticks);

    k_sched_request_switch();
    port_exit_critical(irq);

    /*
     * When this task runs again, timeout processing should have set
     * wait_result to OS_ERR_TIMEOUT.
     */
    if (task->wait_result == OS_ERR_TIMEOUT) {
        return OS_OK;
    }

    return task->wait_result;
}