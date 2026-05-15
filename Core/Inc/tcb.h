#ifndef DOS_INC_TCB_H_
#define DOS_INC_TCB_H_

#include "rtos_config.h"
#include <stdint.h>

typedef enum {
    TaskState_Created = 0U,
    TaskState_Ready,
    TaskState_Running,
    TaskState_Blocked,
    TaskState_Deleted,
    TaskState_MAX_STATE,
} TCB_eTastStates_t;

typedef struct {
    /*
     * MUST be first.
     * Assembly assumes offset 0.
     */
     uint32_t *pu32TaskSP;
     
     uint8_t u8TaskId;
     uint8_t u8TaskPrio;
     TCB_eTastStates_t eTaskState;
     
     uint32_t au32TaskStack[RTOS_TASK_STACK_SIZE];
} TCB_sctTCB_t;

#endif
