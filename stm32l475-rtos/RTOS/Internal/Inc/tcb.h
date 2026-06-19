/*
 * tcb.h
 *  Task control block
 *  Created on: Mar 31, 2025
 *      Author: David
 */

#ifndef DOS_INC_TCB_H_
#define DOS_INC_TCB_H_

#include "os_config.h"
#include <stdint.h>

/// Task state.
typedef enum {
    TaskState_Created = 0U, ///< Task has been created.

    // Normal scheduler states.
    TaskState_Ready,   ///< Ready to be scheduled.
    TaskState_Running, ///< Currently executing.
    TaskState_Blocked, ///< Waiting; not schedulable.

    TaskState_Deleted,   ///< [FUTURE] Deleted; will not run again.
    TaskState_MAX_STATE, ///< Number of states / validity limit.
} TCB_eTaskStates_t;

/// Task control block.
typedef struct {
    uint8_t u8TaskId;             ///< Task ID.
    uint8_t u8TaskPrio;           ///< Task priority.
    TCB_eTaskStates_t eTaskState; ///< Current task state.

    uint32_t au32TaskStack[OS_TASK_STACK_SIZE]; ///< Task stack; stores software-saved context.
    uint32_t *pu32TaskSP;                       ///< Saved stack pointer.
} TCB_sctTCB_t;

#endif /* DOS_INC_TCB_H_ */

/*
TODO: USE THIS WHEN ASSIGNMENT IS DONE

#ifndef TCB_H_
#define TCB_H_

#include "os_config.h"
#include "port.h"

#include <stdint.h>

typedef enum {
    TASK_STATE_CREATED = 0u,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_MAX
} task_state_t;

typedef struct {
    uint8_t id;
    uint8_t priority;
    task_state_t state;

    port_stack_t stack[OS_TASK_STACK_SIZE];
    port_stack_t *stack_ptr;
} tcb_t;

#endif

*/