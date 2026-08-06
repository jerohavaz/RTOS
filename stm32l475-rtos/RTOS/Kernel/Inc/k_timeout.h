/**
 * @file k_timeout.h
 * @brief Kernel tick counter and task-timeout management.
 * @author Jerome
 *
 * Maintains the system tick and the ordered list of blocked tasks with finite
 * timeouts. Tick comparisons remain wrap-safe by restricting delays to less
 * than half of the 32-bit tick range.
 */
#ifndef K_TIMEOUT_H_
#define K_TIMEOUT_H_

#include "kernel_task.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Exclusive upper bound for finite timeout delays.
 *
 * Valid finite delays are in the range 1 through
 * <tt>K_TIMEOUT_MAX - 1</tt> ticks. This half-range limit permits wrap-safe
 * ordering with signed tick subtraction.
 */
#define K_TIMEOUT_MAX 0x80000000u

/**
 * @brief Initialize the tick counter and timeout list.
 *
 * Resets the system tick to zero and initializes an empty timeout list.
 * Intended for kernel startup before tasks can block.
 */
void k_timeout_init(void);

/**
 * @brief Advance the system tick by one.
 *
 * The 32-bit counter wraps naturally at @c UINT32_MAX.
 *
 * @note Called by the SysTick interrupt handler.
 */
void k_tick_inc(void);

/**
 * @brief Return the current system tick.
 *
 * @return Current 32-bit tick value.
 */
uint32_t k_tick_get(void);

/**
 * @brief Add a task to the timeout list.
 *
 * Computes the absolute wake tick from the current tick and @p delay_ticks,
 * then inserts the task in expiration order.
 *
 * @param task Blocked task to register for timeout processing.
 * @param delay_ticks Relative timeout in ticks.
 *
 * @pre @p task must not be 0.
 * @pre @p delay_ticks must be greater than 0 and less than
 *      @ref K_TIMEOUT_MAX.
 * @pre The task must not already be linked in the timeout list and its
 *      @c wake_tick must be zero.
 * @pre The caller must provide the required kernel synchronization.
 */
void k_timeout_add(kernel_task_t *task, uint32_t delay_ticks);

/**
 * @brief Remove a task that must be present in the timeout list.
 *
 * @param task Task to remove.
 *
 * @pre @p task must be linked in the timeout list.
 * @pre The caller must provide the required kernel synchronization.
 *
 * @post The task's @c wake_tick is zero.
 */
void k_timeout_remove(kernel_task_t *task);

/**
 * @brief Remove a task from the timeout list if it is present.
 *
 * @param task Task to remove if currently registered.
 *
 * @retval true The task was removed and its @c wake_tick was cleared.
 * @retval false The task was not registered.
 *
 * @pre @p task must not be 0.
 * @pre The caller must provide the required kernel synchronization.
 */
bool k_timeout_try_remove(kernel_task_t *task);

/**
 * @brief Process every timeout that has expired at the current tick.
 *
 * Removes expired tasks, performs cleanup for each task's wait type, and makes
 * each task ready. Processing occurs inside a kernel critical section.
 *
 * @note This function does not request a context switch. The SysTick handler
 *       requests scheduling after timeout processing is complete.
 */
void k_timeout_process_tick(void);

#endif /* K_TIMEOUT_H_ */