/**
 * @file os_delay.h
 * @brief Task delay services.
 * @author Jerome
 * @author Martin
 *
 * Provides a scheduler-based blocking delay and a CPU-consuming busy delay.
 * Both APIs use the kernel's 32-bit system tick.
 */
#ifndef OS_DELAY_H_
#define OS_DELAY_H_

#include "os_types.h"
#include <stdint.h>

/**
 * @brief Block the current task for a number of system ticks.
 *
 * A nonzero delay blocks the calling task and allows other ready tasks to run.
 * A zero delay does not block; it requests a scheduler yield and returns
 * immediately.
 *
 * @param delay_ticks Delay duration in ticks, or 0 to yield.
 *
 * @retval OS_OK The yield was requested or the delay completed.
 * @retval OS_ERR_IN_ISR Called from exception or interrupt context.
 * @retval OS_ERR_INVALID_ARG @p delay_ticks is greater than or equal to the
 *         kernel's finite-timeout limit.
 * @retval OS_ERR_INVALID_STATE No current task exists or the caller is the
 *         idle task.
 *
 * @note Valid nonzero delays are limited to less than half the 32-bit tick
 *       range so expiration remains correct across tick-counter wraparound.
 */
os_status_t os_delay(uint32_t delay_ticks);

/**
 * @brief Busy-wait for a number of system ticks.
 *
 * Keeps the current task running while repeatedly polling the system tick.
 * Unlike os_delay(), this function does not block or yield the processor.
 * Higher-priority interrupt-driven preemption may still occur.
 *
 * @param delay_ticks Nonzero delay duration in ticks.
 *
 * @retval OS_OK The requested interval elapsed.
 * @retval OS_ERR_IN_ISR Called from exception or interrupt context.
 * @retval OS_ERR_INVALID_ARG @p delay_ticks is 0 or is greater than or equal
 *         to the kernel's finite-timeout limit.
 * @retval OS_ERR_INVALID_STATE No current task exists or the caller is the
 *         idle task.
 *
 * @warning This function consumes CPU time for the full delay. Prefer
 *          os_delay() unless active waiting is specifically required.
 */
os_status_t os_delay_busy(uint32_t delay_ticks);

#endif /* OS_DELAY_H_ */
