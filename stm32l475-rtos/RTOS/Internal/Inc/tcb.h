/**
 * @file tcb.h
 * @brief Architecture-neutral task control block definitions.
 * @author David
 * @author Jerome
 *
 * Defines the task states and control-block fields used by the scheduler and
 * Cortex-M context-switch implementation.
 */

#ifndef DOS_INC_TCB_H_
#define DOS_INC_TCB_H_

#include "os_config.h"

#include <stdint.h>

/**
 * @brief Task lifecycle and scheduler states.
 *
 * A task occupies exactly one state at a time. Only tasks in the ready or
 * running states are schedulable.
 */
typedef enum {
    TaskState_Created = 0U, /**< Task has been created but is not yet ready. */
    TaskState_Ready,        /**< Task is ready to be scheduled. */
    TaskState_Running,      /**< Task is currently executing. */
    TaskState_Blocked,      /**< Task is waiting and cannot be scheduled. */
    TaskState_Deleted,      /**< Reserved for a future deleted-task state. */
    TaskState_MAX_STATE     /**< Number of states and state-validity limit. */
} TCB_eTaskStates_t;

/**
 * @brief Task control block.
 *
 * Stores the task identity, scheduling attributes, statically allocated stack,
 * and saved stack pointer required to resume its execution.
 */
typedef struct {
    uint8_t u8TaskId;             /**< Unique kernel task identifier. */
    uint8_t u8TaskPrio;           /**< Fixed scheduling priority. */
    TCB_eTaskStates_t eTaskState; /**< Current task state. */

    /** Statically allocated task stack containing the saved CPU context. */
    uint32_t au32TaskStack[OS_TASK_STACK_SIZE];

    /** Saved stack pointer used by the context-switch implementation. */
    uint32_t *pu32TaskSP;
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