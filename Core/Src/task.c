#include "task.h"
#include "rtos_config.h"
#include "tcb.h"

void Task_ExitError() {
    while (1) {
    }
}

void Task_Create(TCB_sctTCB_t *psTask,
                 TaskFunction_t pfTaskFunc,
                 uint8_t u8TaskId,
                 uint8_t u8TaskPrio) {
    if ((psTask == 0) || (pfTaskFunc == 0)) {
        return; // TODO: RETURN AN ERR
    }

    uint32_t *sp;

    psTask->u8TaskId = u8TaskId;
    psTask->u8TaskPrio = u8TaskPrio;
    psTask->eTaskState = TaskState_Created;

    sp = &psTask->au32TaskStack[RTOS_TASK_STACK_SIZE];

    *(--sp) = 0x01000000u;              /* xPSR: Thumb bit set, required on Cortex-M */
    *(--sp) = (uint32_t)pfTaskFunc;     /* PC: task entry function */
    *(--sp) = (uint32_t)Task_ExitError; /* LR: called if task function returns */
    *(--sp) = 0x12121212u;              /* R12 */
    *(--sp) = 0x03030303u;              /* R3 */
    *(--sp) = 0x02020202u;              /* R2 */
    *(--sp) = 0x01010101u;              /* R1 */
    *(--sp) = 0x00000000u;              /* R0: first argument to task function, none here */

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