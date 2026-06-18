#include "kernel_task.h"
#include "os_config.h"
#include "k_task.h"
#include "os_task.h"
#include "os_types.h"
#include "port.h"
#include "tcb.h"
#include "trace.h"
#include <stdint.h>

static kernel_task_t g_tasks[OS_MAX_TASKS];
static uint32_t g_task_count = 0u;
static uint8_t g_task_creation_locked = 0u;

static void task_exit_error(void) {
    while (1) {
    }
}

void k_task_init(void) {
    g_task_count = 0u;
    g_task_creation_locked = 0u;

    for (uint32_t i = 0u; i < OS_MAX_TASKS; i++) {
        g_tasks[i].tcb.pu32TaskSP = 0;
        g_tasks[i].tcb.u8TaskId = 0u;
        g_tasks[i].tcb.u8TaskPrio = 0u;
        g_tasks[i].tcb.eTaskState = TaskState_MAX_STATE;
        g_tasks[i].sched_node.next = 0;
        g_tasks[i].sched_node.prev = 0;
        g_tasks[i].timeout_node.next = 0;
        g_tasks[i].timeout_node.prev = 0;
        g_tasks[i].wake_tick = 0u;
        g_tasks[i].wait_object = 0;
        g_tasks[i].wait_result = OS_OK;
    }
}

os_status_t k_task_create_internal(os_task_func_t task_func,
                                   uint8_t prio,
                                   kernel_task_t **out_task) {
    kernel_task_t *task;
    TCB_sctTCB_t *tcb;

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

    task = &g_tasks[g_task_count];
    tcb = &task->tcb;

    tcb->u8TaskId = (uint8_t)g_task_count;
    tcb->u8TaskPrio = prio;
    tcb->eTaskState = TaskState_Created;

    task->sched_node.next = 0;
    task->sched_node.prev = 0;

    task->timeout_node.next = 0;
    task->timeout_node.prev = 0;

    task->wake_tick = 0u;
    task->wait_object = 0;
    task->wait_result = OS_OK;

    tcb->pu32TaskSP =
        port_init_task_stack(tcb->au32TaskStack, OS_TASK_STACK_SIZE, task_func, task_exit_error);

    if (tcb->pu32TaskSP == 0) {
        return OS_ERR_INVALID_STATE;
    }

    g_task_count++;

    trace_task_create(tcb);

    *out_task = task;

    return OS_OK;
}

os_status_t os_task_create(os_task_func_t task_func, uint8_t prio) {
    kernel_task_t *task;
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
    g_task_creation_locked = 1u;
}