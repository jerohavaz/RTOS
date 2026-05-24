#include "os_config.h"
#include "k_task.h"
#include "os_task.h"
#include "os_types.h"
#include "tcb.h"
#include "trace.h"
#include <stdint.h>

static tcb_t g_tasks[OS_MAX_TASKS];
static uint32_t g_task_count = 0u;
static uint8_t g_task_creation_locked = 0u;

static void task_exit_error(void) {
    while (1) {
    }
}

static void task_init_stack(tcb_t *task, os_task_func_t task_func) {
    uint32_t *sp;

    sp = &task->stack[OS_TASK_STACK_SIZE];

    *(--sp) = 0x01000000u;               /* xPSR */
    *(--sp) = (uint32_t)task_func;       /* PC */
    *(--sp) = (uint32_t)task_exit_error; /* LR */
    *(--sp) = 0x12121212u;               /* R12 */
    *(--sp) = 0x03030303u;               /* R3 */
    *(--sp) = 0x02020202u;               /* R2 */
    *(--sp) = 0x01010101u;               /* R1 */
    *(--sp) = 0x00000000u;               /* R0 */

    *(--sp) = 0x11111111u; /* R11 */
    *(--sp) = 0x10101010u; /* R10 */
    *(--sp) = 0x09090909u; /* R9 */
    *(--sp) = 0x08080808u; /* R8 */
    *(--sp) = 0x07070707u; /* R7 */
    *(--sp) = 0x06060606u; /* R6 */
    *(--sp) = 0x05050505u; /* R5 */
    *(--sp) = 0x04040404u; /* R4 */

    task->sp = sp;
}

void k_task_init(void) {
    g_task_count = 0u;
    g_task_creation_locked = 0u;

    for (uint32_t i = 0u; i < OS_MAX_TASKS; i++) {
        g_tasks[i].sp = 0;
        g_tasks[i].id = 0u;
        g_tasks[i].prio = 0u;
        g_tasks[i].state = TASK_STATE_DELETED;
        g_tasks[i].sched_node.next = 0;
        g_tasks[i].sched_node.prev = 0;
    }
}

os_status_t k_task_create_internal(os_task_func_t task_func, uint8_t prio) {
    tcb_t *task;

    if (task_func == 0) {
        return OS_ERR_NULL;
    }

    if (g_task_creation_locked != 0u) {
        return OS_ERR_INVALID_STATE;
    }

    if (prio >= OS_MAX_PRIORITIES) {
        return OS_ERR_INVALID_PRIO;
    }

    if (g_task_count >= OS_MAX_TASKS) {
        return OS_ERR_FULL;
    }

    task = &g_tasks[g_task_count];

    task->id = (uint8_t)g_task_count;
    task->prio = prio;
    task->state = TASK_STATE_CREATED;
    task->sched_node.next = 0;
    task->sched_node.prev = 0;

    task_init_stack(task, task_func);

    g_task_count++;
    trace_task_create(task);

    return OS_OK;
}

os_status_t os_task_create(os_task_func_t task_func, uint8_t prio) {
    if ((prio < OS_USER_MIN_PRIORITY) || (prio > OS_USER_MAX_PRIORITY)) {
        return OS_ERR_INVALID_PRIO;
    }

    return k_task_create_internal(task_func, prio);
}

tcb_t *k_task_get(uint32_t index) {
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