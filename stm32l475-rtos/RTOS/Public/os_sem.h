/**
 * @file os_sem.h
 * @brief Bounded counting semaphores.
 * @author Jerome
 *
 * Provides fixed-maximum counting semaphores with priority-ordered waiters and
 * direct token handoff when a blocked task is released.
 */
#ifndef OS_SEM_H_
#define OS_SEM_H_

#include "os_types.h"
#include "prio_waitq.h"
#include "task_list.h"
#include <stdint.h>

/**
 * @brief Counting-semaphore object.
 *
 * Initialize with os_sem_init() before use. Members are internal semaphore
 * state and must not be modified directly after initialization.
 */
typedef struct {
    uint32_t count;         ///< Number of currently available tokens.
    uint32_t max_count;     ///< Maximum permitted token count.
    prio_waitq_t wait_list; ///< Tasks blocked while acquiring a token.
} os_sem_t;

/**
 * @brief Initialize a bounded counting semaphore.
 *
 * @param sem Semaphore object to initialize.
 * @param initial_count Initial number of available tokens.
 * @param max_count Maximum number of available tokens; must be nonzero.
 *
 * @retval OS_OK Semaphore initialized successfully.
 * @retval OS_ERR_NULL @p sem is 0.
 * @retval OS_ERR_INVALID_ARG @p max_count is 0 or @p initial_count exceeds
 *         @p max_count.
 */
os_status_t os_sem_init(os_sem_t *sem, uint32_t initial_count, uint32_t max_count);

/**
 * @brief Acquire one semaphore token, optionally waiting for availability.
 *
 * If a token is available, decrements the count and returns immediately.
 * Otherwise, the calling task may wait in priority order until a release hands
 * it a token or its finite timeout expires. FIFO order applies among waiters
 * at the same priority.
 *
 * @param sem Initialized semaphore to acquire.
 * @param timeout_ticks Wait policy: @ref OS_NO_WAIT, @ref OS_WAIT_FOREVER, or
 *        a valid nonzero finite timeout in system ticks.
 *
 * @retval OS_OK A token was acquired.
 * @retval OS_ERR_NULL @p sem is 0.
 * @retval OS_ERR_WOULD_BLOCK No token is available and @p timeout_ticks is
 *         @ref OS_NO_WAIT.
 * @retval OS_ERR_IN_ISR The operation would block in exception context.
 * @retval OS_ERR_INVALID_ARG A finite timeout is outside the supported
 *         half-range tick limit.
 * @retval OS_ERR_INVALID_STATE No valid current task exists or the caller is
 *         the idle task when blocking is required.
 * @retval OS_ERR_TIMEOUT The finite wait expired before a token was acquired.
 *
 * @note Exception-context calls are permitted only when a token is immediately
 *       available or when @ref OS_NO_WAIT is used.
 */
os_status_t os_sem_acquire(os_sem_t *sem, uint32_t timeout_ticks);

/**
 * @brief Release one semaphore token.
 *
 * If a task is waiting, transfers the token directly to the highest-priority
 * waiter without incrementing the stored count. Otherwise, increments the
 * count unless it is already at its configured maximum.
 *
 * @param sem Initialized semaphore to release.
 *
 * @retval OS_OK A waiter received the token or the count was incremented.
 * @retval OS_ERR_NULL @p sem is 0.
 * @retval OS_ERR_FULL No task is waiting and the count is already equal to
 *         @c max_count.
 *
 * @note This function may be called from exception or interrupt context.
 */
os_status_t os_sem_release(os_sem_t *sem);

#endif /* OS_SEM_H_ */