#include "os_mutex.h"
#include "k_mutex.h"
#include "k_sched.h"
#include "k_timeout.h"
#include "kernel_panic.h"
#include "os_types.h"
#include "port.h"
#include "prio_waitq.h"

static kernel_task_list_node_t *sched_node(kernel_task_t *task) {
    return &task->sched_node;
}

os_status_t os_mutex_init(os_mutex_t *mutex) {
    if (mutex == 0) {
        return OS_ERR_NULL;
    }

    mutex->owner = 0;
    prio_waitq_init(&mutex->wait_list, sched_node);

    return OS_OK;
}

os_status_t os_mutex_lock(os_mutex_t *mutex, uint32_t timeout_ticks) {
    if (mutex == 0) {
        return OS_ERR_NULL;
    }

    /*
     * Mutexes are task-owned.
     * Exception/ISR context cannot own a mutex and cannot block.
     */
    if (port_in_exception() != 0u) {
        return OS_ERR_IN_ISR;
    }

    if ((timeout_ticks != OS_WAIT_FOREVER) && (timeout_ticks >= 0x80000000u)) {
        return OS_ERR_INVALID_ARG;
    }

    kernel_task_t *current = k_sched_current();

    if (current == 0) {
        return OS_ERR_INVALID_STATE;
    }

    if (k_sched_is_idle(current) != 0u) {
        return OS_ERR_INVALID_STATE;
    }

    uint32_t key = port_enter_critical();

    if (mutex->owner == 0) {
        mutex->owner = current;

        port_exit_critical(key);
        return OS_OK;
    }

    /*
     * Non-recursive mutex:
     * owner may not lock the same mutex again.
     */
    if (mutex->owner == current) {
        port_exit_critical(key);
        return OS_ERR_INVALID_STATE;
    }

    if (timeout_ticks == OS_NO_WAIT) {
        port_exit_critical(key);
        return OS_ERR_WOULD_BLOCK;
    }

    current->wait_object = mutex;
    current->wait_type = K_WAIT_MUTEX;
    current->wait_result = OS_ERR_BUSY;

    prio_waitq_push(&mutex->wait_list, current);

    if (timeout_ticks != OS_WAIT_FOREVER) {
        k_timeout_add(current, timeout_ticks);
    }

    k_sched_task_block(current);

    port_exit_critical(key);

    k_sched_request_switch();

    return current->wait_result;
}

os_status_t os_mutex_unlock(os_mutex_t *mutex) {
    if (mutex == 0) {
        return OS_ERR_NULL;
    }

    /*
     * Only a task can own/unlock a mutex.
     */
    if (port_in_exception() != 0u) {
        return OS_ERR_INVALID_STATE;
    }

    kernel_task_t *current = k_sched_current();

    if (current == 0) {
        return OS_ERR_INVALID_STATE;
    }

    uint32_t key = port_enter_critical();

    if (mutex->owner != current) {
        port_exit_critical(key);
        return OS_ERR_NOT_OWNER;
    }

    kernel_task_t *next = prio_waitq_pop_highest(&mutex->wait_list);

    if (next == 0) {
        mutex->owner = 0;

        port_exit_critical(key);
        return OS_OK;
    }

    /*
     * Direct handoff:
     * ownership moves directly from current owner to next waiter.
     * Do not set owner to 0 in between.
     */
    k_timeout_try_remove(next);

    mutex->owner = next;

    next->wait_type = K_WAIT_NONE;
    next->wait_object = 0;
    next->wait_result = OS_OK;

    k_sched_task_ready(next);

    port_exit_critical(key);

    k_sched_request_switch();

    return OS_OK;
}

void k_mutex_timeout_cleanup(os_mutex_t *mutex, kernel_task_t *task) {
    KERNEL_REQUIRE(mutex != 0);
    KERNEL_REQUIRE(task != 0);
    KERNEL_REQUIRE(task->wait_type == K_WAIT_MUTEX);
    KERNEL_REQUIRE(task->wait_object == mutex);

    prio_waitq_remove(&mutex->wait_list, task);

    task->wait_type = K_WAIT_NONE;
    task->wait_object = 0;
    task->wait_result = OS_ERR_TIMEOUT;
}