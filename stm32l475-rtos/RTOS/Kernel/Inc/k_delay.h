/**
 * @file k_delay.h
 * @brief Internal task-delay timeout cleanup.
 * @author Jerome
 *
 * @details
 * Declares the cleanup hook used by the timeout subsystem when an
 * @c os_delay() wait expires. This is an internal kernel interface; application
 * code should use @c os_delay() instead.
 */

#ifndef K_DELAY_H_
#define K_DELAY_H_

#include "kernel_task.h"
#include "os_sem.h"
#include "task_list.h"

/**
 * @brief Complete a task delay after its timeout expires.
 *
 * Clears the task's delay-wait metadata and sets its wait result to
 * @c OS_OK. The timeout subsystem removes the task from the timeout list before
 * calling this function and readies it afterward.
 *
 * @param task Delayed task whose timeout has expired.
 *
 * @pre @p task must not be null.
 * @pre @c task->wait_type must equal @c K_WAIT_DELAY.
 * @pre @c task->wait_object must be null.
 * @post @c task->wait_type equals @c K_WAIT_NONE.
 * @post @c task->wait_object is null.
 * @post @c task->wait_result equals @c OS_OK.
 *
 * @note This function updates wait metadata only; it does not remove the
 *       timeout entry or make the task ready.
 */
void k_delay_timeout_cleanup(kernel_task_t *task);

#endif /* K_DELAY_H_ */
