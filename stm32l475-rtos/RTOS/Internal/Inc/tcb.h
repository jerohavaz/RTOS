/**
 * @file tcb.h
 * @brief Architecture-neutral task control block definitions.
 * @author David
 * @author Jerome
 *
 * Defines the task states and control-block fields used by the scheduler and
 * Cortex-M context-switch implementation.
 */

#ifndef TCB_H_
#define TCB_H_

#include "os_config.h"
#include "port.h"

#include <stdint.h>

/**
 * @brief Task lifecycle and scheduler states.
 *
 * A task occupies exactly one state at a time. Only tasks in the ready or
 * running states are schedulable.
 */
typedef enum {
    TASK_STATE_CREATED = 0u, /**< Task has been created but is not yet ready. */
    TASK_STATE_READY,        /**< Task is ready to be scheduled. */
    TASK_STATE_RUNNING,      /**< Task is currently executing. */
    TASK_STATE_BLOCKED,      /**< Task is waiting and cannot be scheduled. */
    TASK_STATE_MAX           /**< Number of states and state-validity limit. */
} task_state_t;

/**
 * @brief Task control block.
 *
 * Stores the task identity, scheduling attributes, statically allocated stack,
 * and saved stack pointer required to resume task execution.
 */
typedef struct {
    uint8_t id;         /**< Unique kernel task identifier. */
    uint8_t priority;   /**< Fixed scheduling priority. */
    task_state_t state; /**< Current task state. */

    /** Statically allocated task stack containing the saved CPU context. */
    port_stack_t stack[OS_TASK_STACK_SIZE];

    /** Saved stack pointer used by the context-switch implementation. */
    port_stack_t *stack_ptr;
} tcb_t;

#endif /* TCB_H_ */