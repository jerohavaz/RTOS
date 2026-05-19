#include "task.h"
#include "rtos_config.h"
#include "idle.h"
#include <stdint.h>

static TCB_sctTCB_t g_asTaskList[RTOS_MAX_TASKS];
static uint32_t g_u32TaskCount = 0u;
static uint8_t g_u8TaskCreationLocked = 0u;

static void Task_ExitError(void) {
    while (1) {
    }
}

static void KERNEL_TaskInitStack(TCB_sctTCB_t *psTask, TaskFunction_t pfTaskFunc) {
    uint32_t *sp;

    sp = &psTask->au32TaskStack[RTOS_TASK_STACK_SIZE];

    *(--sp) = 0x01000000u;              /* xPSR */
    *(--sp) = (uint32_t)pfTaskFunc;     /* PC */
    *(--sp) = (uint32_t)Task_ExitError; /* LR */
    *(--sp) = 0x12121212u;              /* R12 */
    *(--sp) = 0x03030303u;              /* R3 */
    *(--sp) = 0x02020202u;              /* R2 */
    *(--sp) = 0x01010101u;              /* R1 */
    *(--sp) = 0x00000000u;              /* R0 */

    *(--sp) = 0x11111111u; /* R11 */
    *(--sp) = 0x10101010u; /* R10 */
    *(--sp) = 0x09090909u; /* R9 */
    *(--sp) = 0x08080808u; /* R8 */
    *(--sp) = 0x07070707u; /* R7 */
    *(--sp) = 0x06060606u; /* R6 */
    *(--sp) = 0x05050505u; /* R5 */
    *(--sp) = 0x04040404u; /* R4 */

    psTask->pu32TaskSP = sp;
}

void KERNEL_TaskSystemInit(void) {
    g_u32TaskCount = 0u;
    g_u8TaskCreationLocked = 0u;

    for (uint32_t i = 0u; i < RTOS_MAX_TASKS; i++) {
        g_asTaskList[i].pu32TaskSP = 0;
        g_asTaskList[i].u8TaskId = 0u;
        g_asTaskList[i].u8TaskPrio = 0u;
        g_asTaskList[i].eTaskState = TaskState_Deleted;
    }
}

RTOS_Status_t KERNEL_TaskCreateInternal(TaskFunction_t pfTaskFunc, uint8_t u8TaskPrio) {
    TCB_sctTCB_t *psTask;

    if (pfTaskFunc == 0) {
        return RTOS_ERR_NULL;
    }

    if (g_u8TaskCreationLocked != 0u) {
        return RTOS_ERR_INVALID_STATE;
    }

    if (u8TaskPrio >= RTOS_MAX_PRIORITIES) {
        return RTOS_ERR_INVALID_PRIO;
    }

    if (g_u32TaskCount >= RTOS_MAX_TASKS) {
        return RTOS_ERR_FULL;
    }

    psTask = &g_asTaskList[g_u32TaskCount];

    psTask->u8TaskId = (uint8_t)g_u32TaskCount;
    psTask->u8TaskPrio = u8TaskPrio;
    psTask->eTaskState = TaskState_Created;

    KERNEL_TaskInitStack(psTask, pfTaskFunc);

    g_u32TaskCount++;

    return RTOS_OK;
}

RTOS_Status_t RTOS_TaskCreate(TaskFunction_t pfTaskFunc, uint8_t u8TaskPrio) {
    if ((u8TaskPrio < RTOS_USER_MIN_PRIORITY) || (u8TaskPrio > RTOS_USER_MAX_PRIORITY)) {
        return RTOS_ERR_INVALID_PRIO;
    }

    return KERNEL_TaskCreateInternal(pfTaskFunc, u8TaskPrio);
}

TCB_sctTCB_t *KERNEL_TaskGetByIndex(uint32_t u32Index) {
    if (u32Index >= g_u32TaskCount) {
        return 0;
    }

    return &g_asTaskList[u32Index];
}

uint32_t KERNEL_TaskGetCount(void) {
    return g_u32TaskCount;
}

void KERNEL_TaskLockCreation(void) {
    g_u8TaskCreationLocked = 1u;
}