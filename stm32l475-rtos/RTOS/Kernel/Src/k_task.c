/**
 * @file k_task.c
 * @brief Static task storage and creation implementation.
 * @author Jerome
 */

#include "kernel_task.h"
#include "kernel_panic.h"
#include "os_config.h"
#include "k_task.h"
#include "k_trace.h"
#include "os_task.h"
#include "os_types.h"
#include "port.h"
#include "tcb.h"
#include "trace.h"
#include <stdbool.h>
#include <stdint.h>

#if (K_MAX_TASKS == 0u)
#error "K_MAX_TASKS must be greater than 0"
#endif

#if (K_MAX_TASKS > 255u)
#error "K_MAX_TASKS must fit in uint8_t task IDs"
#endif

static kernel_task_t g_tasks[K_MAX_TASKS] __attribute__((section(".ram2_bss"), aligned(8)));
static uint32_t g_task_count = 0u;
static bool g_task_creation_locked = false;

/**
 * @brief Trap a task that returns from its entry function.
 *
 * Installed as the initial task LR by port_init_task_stack(). A valid RTOS
 * task is not permitted to return, so execution remains in this loop.
 *
 * @note This function never returns.
 */
static void task_exit_error(void) {
    while (1) {
    }
}

/**
 * @brief Reset a task slot to its unused state.
 *
 * Clears the TCB state, intrusive-list links, wake tick, wait type, wait
 * object, and wait result so the slot can be initialized by task creation.
 *
 * @param task Task slot to clear.
 * @pre @p task must not be 0.
 */
static void task_clear(kernel_task_t *task) {
    KERNEL_REQUIRE(task != 0);

    task->tcb.stack_ptr = 0;
    task->tcb.id = 0u;
    task->tcb.priority = 0u;
    task->tcb.state = TASK_STATE_MAX;

    task->sched_node.next = 0;
    task->sched_node.prev = 0;

    task->timeout_node.next = 0;
    task->timeout_node.prev = 0;

    task->wake_tick = 0u;
    task->wait_type = K_WAIT_NONE;
    task->wait_object = 0;
    task->wait_result = OS_OK;
    task->wait_data = 0;
}

void k_task_init(void) {
    g_task_count = 0u;
    g_task_creation_locked = false;

    for (uint32_t i = 0u; i < K_MAX_TASKS; i++) {
        task_clear(&g_tasks[i]);
    }
}

os_status_t k_task_create_internal(os_task_func_t task_func,
                                   uint8_t prio,
                                   bool is_idle,
                                   kernel_task_t **out_task) {
    if (out_task == 0) {
        return OS_ERR_NULL;
    }

    *out_task = 0;

    if (task_func == 0) {
        return OS_ERR_NULL;
    }

    if (g_task_creation_locked) {
        return OS_ERR_INVALID_STATE;
    }

    if (prio > OS_TASK_PRIORITY_HIGHEST) {
        return OS_ERR_INVALID_PRIO;
    }

    if (g_task_count >= K_MAX_TASKS) {
        return OS_ERR_FULL;
    }

    KERNEL_REQUIRE(g_task_count < K_MAX_TASKS);

    kernel_task_t *task = &g_tasks[g_task_count];
    tcb_t *tcb = &task->tcb;

    KERNEL_REQUIRE(task != 0);

    KERNEL_REQUIRE(task->tcb.stack_ptr == 0);
    KERNEL_REQUIRE(task->tcb.state == TASK_STATE_MAX);

    KERNEL_REQUIRE(task->sched_node.next == 0);
    KERNEL_REQUIRE(task->sched_node.prev == 0);

    KERNEL_REQUIRE(task->timeout_node.next == 0);
    KERNEL_REQUIRE(task->timeout_node.prev == 0);

    tcb->id = (uint8_t)g_task_count;
    tcb->priority = prio;
    tcb->state = TASK_STATE_CREATED;

    tcb->stack_ptr =
        port_init_task_stack(tcb->stack, OS_TASK_STACK_SIZE, task_func, task_exit_error);

    if (tcb->stack_ptr == 0) {
        task_clear(task);
        return OS_ERR_INVALID_STATE;
    }

    g_task_count++;

    trace_task_info_t trace_info =
        k_trace_task_info(task, is_idle ? TRACE_TASK_KIND_IDLE : TRACE_TASK_KIND_NORMAL);
    trace_task_register(&trace_info);

    *out_task = task;

    return OS_OK;
}

os_status_t os_task_create(os_task_func_t task_func, uint8_t prio) {
    kernel_task_t *task = 0;

    return k_task_create_internal(task_func, prio, false, &task);
}

kernel_task_t *k_task_get(uint32_t index) {
    if (index >= g_task_count) {
        return 0;
    }

    return &g_tasks[index];
}

uint32_t k_task_count(void) {
    return g_task_count;
}

void k_task_lock_creation(void) {
    KERNEL_REQUIRE(!g_task_creation_locked);

    g_task_creation_locked = true;
}
