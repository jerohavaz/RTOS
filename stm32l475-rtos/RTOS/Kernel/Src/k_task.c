#include "kernel_task.h"
#include "kernel_panic.h"
#include "os_config.h"
#include "k_task.h"
#include "os_task.h"
#include "os_types.h"
#include "port.h"
#include "tcb.h"
#include "trace.h"
#include <stdint.h>

#if (OS_MAX_TASKS == 0u)
#error "OS_MAX_TASKS must be greater than 0"
#endif

#if (OS_MAX_TASKS > 255u)
#error "OS_MAX_TASKS must fit in uint8_t task IDs"
#endif

static kernel_task_t g_tasks[OS_MAX_TASKS];
static uint32_t g_task_count = 0u;
static uint8_t g_task_creation_locked = 0u;

static void task_exit_error(void) {
    while (1) {
    }
}

static void task_clear(kernel_task_t *task) {
    KERNEL_REQUIRE(task != 0);

    task->tcb.pu32TaskSP = 0;
    task->tcb.u8TaskId = 0u;
    task->tcb.u8TaskPrio = 0u;
    task->tcb.eTaskState = TaskState_MAX_STATE;

    task->sched_node.next = 0;
    task->sched_node.prev = 0;

    task->timeout_node.next = 0;
    task->timeout_node.prev = 0;

    task->wake_tick = 0u;
    task->wait_type = K_WAIT_NONE;
    task->wait_object = 0;
    task->wait_result = OS_OK;
}

void k_task_init(void) {
    g_task_count = 0u;
    g_task_creation_locked = 0u;

    for (uint32_t i = 0u; i < OS_MAX_TASKS; i++) {
        task_clear(&g_tasks[i]);
    }
}

os_status_t k_task_create_internal(os_task_func_t task_func,
                                   uint8_t prio,
                                   kernel_task_t **out_task) {
    if (out_task == 0) {
        return OS_ERR_NULL;
    }

    *out_task = 0;

    if (task_func == 0) {
        return OS_ERR_NULL;
    }

    if (g_task_creation_locked != 0u) {
        return OS_ERR_INVALID_STATE;
    }

    if (prio > OS_TASK_PRIORITY_HIGHEST) {
        return OS_ERR_INVALID_PRIO;
    }

    if (g_task_count >= OS_MAX_TASKS) {
        return OS_ERR_FULL;
    }

    KERNEL_REQUIRE(g_task_count < OS_MAX_TASKS);

    kernel_task_t *task = &g_tasks[g_task_count];
    TCB_sctTCB_t *tcb = &task->tcb;

    KERNEL_REQUIRE(task != 0);

    KERNEL_REQUIRE(task->tcb.pu32TaskSP == 0);
    KERNEL_REQUIRE(task->tcb.eTaskState == TaskState_MAX_STATE);

    KERNEL_REQUIRE(task->sched_node.next == 0);
    KERNEL_REQUIRE(task->sched_node.prev == 0);

    KERNEL_REQUIRE(task->timeout_node.next == 0);
    KERNEL_REQUIRE(task->timeout_node.prev == 0);

    tcb->u8TaskId = (uint8_t)g_task_count;
    tcb->u8TaskPrio = prio;
    tcb->eTaskState = TaskState_Created;

    tcb->pu32TaskSP =
        port_init_task_stack(tcb->au32TaskStack, OS_TASK_STACK_SIZE, task_func, task_exit_error);

    if (tcb->pu32TaskSP == 0) {
        task_clear(task);
        return OS_ERR_INVALID_STATE;
    }

    g_task_count++;

    trace_task_create(tcb);

    *out_task = task;

    return OS_OK;
}

os_status_t os_task_create(os_task_func_t task_func, uint8_t prio) {
    kernel_task_t *task = 0;

    return k_task_create_internal(task_func, prio, &task);
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
    KERNEL_REQUIRE(g_task_creation_locked == 0u);

    g_task_creation_locked = 1u;
}