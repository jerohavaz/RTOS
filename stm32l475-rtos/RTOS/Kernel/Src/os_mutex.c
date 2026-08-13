/**
 * @file os_mutex.c
 * @brief Mutex API and timeout-cleanup implementation.
 * @author Jerome
 */

#include "os_mutex.h"
#include "k_mutex.h"
#include "k_sched.h"
#include "k_timeout.h"
#include "k_trace.h"
#include "kernel_panic.h"
#include "os_types.h"
#include "port.h"
#include "prio_waitq.h"
#include "trace.h"

/**
 * @brief Select a task's scheduler node for the mutex wait queue.
 *
 * @param task Task whose embedded node is required.
 * @return Pointer to @p task's scheduler node.
 * @pre @p task must not be 0.
 */
static kernel_task_list_node_t *sched_node(kernel_task_t *task) {
    return &task->sched_node;
}

os_status_t os_mutex_init(os_mutex_t *mutex) {
    if (mutex == 0) {
        return OS_ERR_NULL;
    }

    mutex->owner = 0;
    prio_waitq_init(&mutex->wait_list, sched_node);

    trace_mutex_create(mutex);

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
    if (port_in_exception()) {
        return OS_ERR_IN_ISR;
    }

    if ((timeout_ticks != OS_WAIT_FOREVER) && (timeout_ticks >= K_TIMEOUT_MAX)) {
        return OS_ERR_INVALID_ARG;
    }

    kernel_task_t *current = k_sched_current();

    if (current == 0 || k_sched_is_idle(current)) {
        return OS_ERR_INVALID_STATE;
    }

    uint32_t key = port_enter_critical();
    uint8_t finite_timeout = (uint8_t)(timeout_ticks != OS_WAIT_FOREVER);

    trace_mutex_lock_enter(mutex,
                           k_trace_task_ref(current),
                           k_trace_task_ref(mutex->owner),
                           timeout_ticks,
                           finite_timeout);

    if (mutex->owner == 0) {
        mutex->owner = current;

        trace_mutex_lock_exit(mutex, k_trace_task_ref(current), k_trace_task_ref(current), 1u);

        port_exit_critical(key);
        return OS_OK;
    }

    /*
     * Non-recursive mutex:
     * owner may not lock the same mutex again.
     */
    if (mutex->owner == current) {
        trace_mutex_lock_exit(mutex, k_trace_task_ref(current), k_trace_task_ref(current), 0u);
        port_exit_critical(key);
        return OS_ERR_INVALID_STATE;
    }

    if (timeout_ticks == OS_NO_WAIT) {
        trace_mutex_lock_exit(mutex, k_trace_task_ref(current), k_trace_task_ref(mutex->owner), 0u);
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

    trace_mutex_block(mutex,
                      k_trace_task_ref(current),
                      k_trace_task_ref(mutex->owner),
                      timeout_ticks,
                      finite_timeout);
    k_sched_task_block(current);

    port_exit_critical(key);

    k_sched_request_switch();

    key = port_enter_critical();
    os_status_t result = current->wait_result;
    trace_mutex_lock_exit(mutex,
                          k_trace_task_ref(current),
                          k_trace_task_ref(mutex->owner),
                          (uint8_t)(result == OS_OK));
    port_exit_critical(key);

    return result;
}

os_status_t os_mutex_unlock(os_mutex_t *mutex) {
    if (mutex == 0) {
        return OS_ERR_NULL;
    }

    /*
     * Only a task can own/unlock a mutex.
     */
    if (port_in_exception()) {
        return OS_ERR_IN_ISR;
    }

    kernel_task_t *current = k_sched_current();

    if (current == 0) {
        return OS_ERR_INVALID_STATE;
    }

    uint32_t key = port_enter_critical();
    trace_task_ref_t current_trace = k_trace_task_ref(current);
    trace_task_ref_t owner_before = k_trace_task_ref(mutex->owner);

    if (mutex->owner != current) {
        trace_mutex_unlock(mutex, current_trace, owner_before, owner_before, 0u);
        port_exit_critical(key);
        return OS_ERR_NOT_OWNER;
    }

    kernel_task_t *next = prio_waitq_pop_highest(&mutex->wait_list);

    if (next == 0) {
        mutex->owner = 0;

        trace_mutex_unlock(mutex, current_trace, owner_before, trace_task_ref_none(), 1u);

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

    trace_mutex_unlock(mutex, current_trace, owner_before, k_trace_task_ref(next), 1u);
    trace_mutex_wake(mutex, k_trace_task_ref(next));
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

    trace_mutex_timeout(mutex, k_trace_task_ref(task), k_trace_task_ref(mutex->owner));

    task->wait_type = K_WAIT_NONE;
    task->wait_object = 0;
    task->wait_result = OS_ERR_TIMEOUT;
}