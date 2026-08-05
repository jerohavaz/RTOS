/**
 * @file k_idle.h
 * @brief Internal idle-task creation and entry point.
 * @author Jerome
 *
 * @details
 * Declares the kernel helpers used to create and run the scheduler's dedicated
 * lowest-priority idle task.
 */

#ifndef K_IDLE_H_
#define K_IDLE_H_

/**
 * @brief Create and register the kernel idle task.
 *
 * Creates @ref k_idle_task at @c OS_TASK_PRIORITY_LOWEST and registers the
 * resulting task object with the scheduler.
 *
 * @pre The task subsystem and scheduler must be initialized.
 * @pre Task creation must still be unlocked.
 * @pre No idle task may already be registered.
 * @post The scheduler has one valid idle task.
 *
 * @note Creation failure or an invalid returned task triggers a kernel panic.
 */
void k_idle_create(void);

/**
 * @brief Run the non-returning idle loop.
 *
 * When tracing is enabled, executes no-operation instructions to keep debugger
 * and RTT access active. Otherwise, waits for interrupts to reduce idle CPU
 * activity.
 *
 * @note This task never returns.
 */
void k_idle_task(void);

#endif /* K_IDLE_H_ */