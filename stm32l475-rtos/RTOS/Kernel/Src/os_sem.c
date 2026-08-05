/**
 * @file os_sem.c
 * @brief Semaphore API and timeout-cleanup implementation.
 * @author Jerome
 */

#include "k_sched.h"
#include "k_sem.h"
#include "k_timeout.h"
#include "kernel_panic.h"
#include "port.h"
#include "trace.h"
#include "os_sem.h"

#include <stdbool.h>

/**
 * @brief Select a task's scheduler node for the semaphore wait queue.
 *
 * @param task Task whose embedded node is required.
 * @return Pointer to @p task's scheduler node.
 * @pre @p task must not be 0.
 */
static kernel_task_list_node_t *sched_node(kernel_task_t *task) {
    return &task->sched_node;
}

os_status_t os_sem_init(os_sem_t *sem, uint32_t initial_count, uint32_t max_count) {
    if (sem == 0) {
        return OS_ERR_NULL;
    }

    if ((max_count == 0u) || (initial_count > max_count)) {
        return OS_ERR_INVALID_ARG;
    }

    sem->count = initial_count;
    sem->max_count = max_count;

    prio_waitq_init(&sem->wait_list, sched_node);

    trace_sem_create(sem, initial_count, max_count);

    return OS_OK;
}

os_status_t os_sem_acquire(os_sem_t *sem, uint32_t timeout_ticks) {
    if (sem == 0) {
        return OS_ERR_NULL;
    }

    /*
     * timeout_list ordering uses signed tick subtraction, so delays must stay
     * below 2^31 ticks.
     */
    if ((timeout_ticks != OS_WAIT_FOREVER) && (timeout_ticks >= K_TIMEOUT_MAX)) {
        return OS_ERR_INVALID_ARG;
    }

    uint32_t key = port_enter_critical();
    bool in_exception = port_in_exception();
    kernel_task_t *current = in_exception ? 0 : k_sched_current();
    TCB_sctTCB_t *current_tcb = (current != 0) ? &current->tcb : 0;
    uint8_t finite_timeout = (uint8_t)(timeout_ticks != OS_WAIT_FOREVER);

    trace_sem_acquire_enter(
        sem, current_tcb, sem->count, timeout_ticks, finite_timeout);

    if (sem->count != 0u) {
        sem->count--;
        trace_sem_acquire_exit(sem, current_tcb, sem->count, 1u);
        port_exit_critical(key);
        return OS_OK;
    }

    if (timeout_ticks == OS_NO_WAIT) {
        trace_sem_acquire_exit(sem, current_tcb, sem->count, 0u);
        port_exit_critical(key);
        return OS_ERR_WOULD_BLOCK;
    }

    if (in_exception) {
        trace_sem_acquire_exit(sem, 0, sem->count, 0u);
        port_exit_critical(key);
        return OS_ERR_IN_ISR;
    }

    if (current == 0 || k_sched_is_idle(current)) {
        trace_sem_acquire_exit(sem, current_tcb, sem->count, 0u);
        port_exit_critical(key);
        return OS_ERR_INVALID_STATE;
    }

    current->wait_object = sem;
    current->wait_type = K_WAIT_SEM;
    current->wait_result = OS_ERR_BUSY;

    prio_waitq_push(&sem->wait_list, current);

    if (timeout_ticks != OS_WAIT_FOREVER) {
        k_timeout_add(current, timeout_ticks);
    }

    trace_sem_block(sem, &current->tcb, timeout_ticks, finite_timeout);
    k_sched_task_block(current);
    port_exit_critical(key);

    k_sched_request_switch();

    key = port_enter_critical();
    os_status_t result = current->wait_result;
    trace_sem_acquire_exit(sem, &current->tcb, sem->count, (uint8_t)(result == OS_OK));
    port_exit_critical(key);

    return result;
}

os_status_t os_sem_release(os_sem_t *sem) {
    if (sem == 0) {
        return OS_ERR_NULL;
    }

    uint32_t key = port_enter_critical();
    uint32_t count_before = sem->count;
    kernel_task_t *task = prio_waitq_pop_highest(&sem->wait_list);

    if (task == 0) {
        if (sem->count >= sem->max_count) {
            trace_sem_release(sem, count_before, sem->count, sem->max_count, 0u);
            port_exit_critical(key);
            return OS_ERR_FULL;
        }

        sem->count++;
        trace_sem_release(sem, count_before, sem->count, sem->max_count, 1u);

        port_exit_critical(key);
        return OS_OK;
    }

    /*
     * Direct handoff:
     * The released token goes to the waiting task.
     * Do not increment sem->count here.
     */
    k_timeout_try_remove(task);

    task->wait_object = 0;
    task->wait_type = K_WAIT_NONE;
    task->wait_result = OS_OK;

    trace_sem_release(sem, count_before, sem->count, sem->max_count, 1u);
    trace_sem_wake(sem, &task->tcb);
    k_sched_task_ready(task);
    port_exit_critical(key);

    k_sched_request_switch();

    return OS_OK;
}

void k_sem_timeout_cleanup(os_sem_t *sem, kernel_task_t *task) {
    KERNEL_REQUIRE(sem != 0);
    KERNEL_REQUIRE(task != 0);
    KERNEL_REQUIRE(task->wait_type == K_WAIT_SEM);
    KERNEL_REQUIRE(task->wait_object == sem);

    prio_waitq_remove(&sem->wait_list, task);

    trace_sem_timeout(sem, &task->tcb, sem->count);

    task->wait_type = K_WAIT_NONE;
    task->wait_object = 0;
    task->wait_result = OS_ERR_TIMEOUT;
}
