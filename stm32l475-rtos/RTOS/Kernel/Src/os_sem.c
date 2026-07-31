#include "k_sched.h"
#include "k_sem.h"
#include "k_timeout.h"
#include "kernel_panic.h"
#include "port.h"
#include "os_sem.h"

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

    return OS_OK;
}

os_status_t os_sem_acquire(os_sem_t *sem, uint32_t timeout_ticks) {
    if (sem == 0) {
        return OS_ERR_NULL;
    }

    uint32_t key = port_enter_critical();

    if (sem->count != 0u) {
        sem->count--;
        port_exit_critical(key);
        return OS_OK;
    }

    if (timeout_ticks == OS_NO_WAIT) {
        port_exit_critical(key);
        return OS_ERR_WOULD_BLOCK;
    }

    if (port_in_exception()) {
        port_exit_critical(key);
        return OS_ERR_IN_ISR;
    }

    /*
     * timeout_list ordering uses signed tick subtraction, so delays must stay
     * below 2^31 ticks.
     */
    if ((timeout_ticks != OS_WAIT_FOREVER) && (timeout_ticks >= K_TIMEOUT_MAX)) {
        port_exit_critical(key);
        return OS_ERR_INVALID_ARG;
    }

    kernel_task_t *current = k_sched_current();

    if (current == 0 || k_sched_is_idle(current)) {
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

    k_sched_task_block(current);
    port_exit_critical(key);

    k_sched_request_switch();

    return current->wait_result;
}

os_status_t os_sem_release(os_sem_t *sem) {
    if (sem == 0) {
        return OS_ERR_NULL;
    }

    uint32_t key = port_enter_critical();
    kernel_task_t *task = prio_waitq_pop_highest(&sem->wait_list);

    if (task == 0) {
        if (sem->count >= sem->max_count) {
            port_exit_critical(key);
            return OS_ERR_FULL;
        }

        sem->count++;

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

    task->wait_type = K_WAIT_NONE;
    task->wait_object = 0;
    task->wait_result = OS_ERR_TIMEOUT;
}