/**
 * @file k_sched.h
 * @brief Internal fixed-priority scheduler interface.
 * @author Jerome
 *
 * @details
 * Declares scheduler lifecycle, task-state transitions, preemption requests,
 * and the stack-pointer exchange functions called by the Cortex-M SVC and
 * PendSV handlers. Larger numeric task priorities are scheduled before smaller
 * priorities; equal-priority tasks rotate only on an explicit yield or tick.
 */

#ifndef K_SCHED_H_
#define K_SCHED_H_

#include "kernel_task.h"
#include "port.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Reset scheduler state and initialize the ready queue and port IRQs.
 *
 * @post No current or idle task is registered and the scheduler is stopped.
 * @post The priority ready queue is empty.
 */
void k_sched_init(void);

/**
 * @brief Start scheduling and transfer control to the first task.
 *
 * Moves every created task to the ready state, selects the highest-priority
 * task or idle fallback, and enters the architecture-specific startup path.
 *
 * @pre A valid idle task must be registered.
 * @note This function does not return during normal operation.
 */
void k_sched_start(void);

/**
 * @brief Register the scheduler's dedicated idle task.
 *
 * @param task Valid task object created for the idle loop.
 *
 * @pre @p task must not be null.
 * @pre No idle task may already be registered.
 */
void k_sched_set_idle_task(kernel_task_t *task);

/**
 * @brief Test whether a task is the registered idle task.
 *
 * @param task Task to inspect; null is permitted.
 *
 * @retval true @p task is the registered idle task.
 * @retval false @p task is null or is not the idle task.
 */
bool k_sched_is_idle(const kernel_task_t *task);

/**
 * @brief Commit scheduler startup and return the first task context.
 *
 * Called by the SVC assembly handler. Marks the selected task running, marks
 * the scheduler started, and returns the task's initialized saved SP.
 *
 * @return Non-null saved stack pointer for the first task.
 *
 * @pre A current task with a valid saved stack pointer must be selected.
 */
port_stack_t *k_sched_start_first_context(void);

/**
 * @brief Save the outgoing context reference and select the next task context.
 *
 * Called by the PendSV assembly handler after saving the outgoing task's
 * software context. A still-running outgoing task is returned to the ready
 * queue; a task that already blocked is not reinserted.
 *
 * @param outgoing_sp Saved stack pointer of the outgoing task.
 *
 * @return Non-null saved stack pointer of the selected incoming task.
 *
 * @pre @p outgoing_sp must not be null.
 * @pre The scheduler must have a valid current task and idle fallback.
 */
port_stack_t *k_sched_switch_context(port_stack_t *outgoing_sp);

/**
 * @brief Move a task to the ready state.
 *
 * Non-idle tasks are appended to their priority's FIFO ready list. The
 * function protects the transition with a kernel critical section.
 *
 * @param task Task to make ready.
 * @pre @p task must not be null.
 */
void k_sched_task_ready(kernel_task_t *task);

/**
 * @brief Move a non-idle task to the blocked state.
 *
 * @param task Task that has begun waiting.
 * @pre @p task must not be null and must not be the idle task.
 */
void k_sched_task_block(kernel_task_t *task);

/**
 * @brief Request preemption when higher-priority work is ready.
 *
 * Pends a context switch when the current task blocked, idle must yield to real
 * work, or a strictly higher-priority task is ready. Equal-priority tasks do
 * not rotate through this function.
 *
 * @retval true A context switch was requested.
 * @retval false The scheduler is stopped or the current task may continue.
 */
bool k_sched_request_switch(void);

/**
 * @brief Request a scheduling yield, including equal-priority rotation.
 *
 * Applies the normal preemption rules and additionally requests a switch when
 * another task of the current priority is ready.
 *
 * @retval true A context switch was requested.
 * @retval false The scheduler is stopped or no eligible replacement exists.
 */
bool k_sched_request_yield(void);

/**
 * @brief Return the scheduler's current task.
 *
 * Reads the current-task pointer inside a kernel critical section.
 *
 * @return Current task.
 * @retval NULL No current task has been selected yet.
 */
kernel_task_t *k_sched_current(void);

/**
 * @brief Test whether first-task context startup has completed.
 *
 * @retval true The SVC startup path committed the first running task.
 * @retval false The scheduler has not started task execution.
 */
bool k_sched_started(void);

#endif /* K_SCHED_H_ */