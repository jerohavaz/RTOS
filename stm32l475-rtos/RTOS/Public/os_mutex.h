/**
 * @file os_mutex.h
 * @brief Task-owned mutual-exclusion mutexes.
 * @author Jerome
 *
 * Provides non-recursive mutexes with priority-ordered waiters and direct
 * ownership handoff on unlock. Mutex operations do not implement priority
 * inheritance.
 */
#ifndef OS_MUTEX_H_
#define OS_MUTEX_H_

#include "kernel_task.h"
#include "os_types.h"
#include "prio_waitq.h"
#include <stdint.h>

/**
 * @brief Mutex object.
 *
 * Objects must be initialized with os_mutex_init() before use.
 */
typedef struct {
    kernel_task_t *owner;   ///< Current owner, or 0 when unlocked.
    prio_waitq_t wait_list; ///< Tasks blocked while waiting for ownership.
} os_mutex_t;

/**
 * @brief Initialize an unlocked mutex.
 *
 * @param mutex Mutex object to initialize.
 *
 * @retval OS_OK Mutex initialized successfully.
 * @retval OS_ERR_NULL @p mutex is 0.
 */
os_status_t os_mutex_init(os_mutex_t *mutex);

/**
 * @brief Acquire a mutex, optionally waiting for ownership.
 *
 * If the mutex is available, the current task becomes its owner immediately.
 * Otherwise, the task either returns without blocking or waits in priority
 * order until ownership is handed off or the finite timeout expires.
 *
 * @param mutex Initialized mutex to acquire.
 * @param timeout_ticks Wait policy: @ref OS_NO_WAIT, @ref OS_WAIT_FOREVER, or
 *        a valid nonzero finite timeout in system ticks.
 *
 * @retval OS_OK The calling task acquired the mutex.
 * @retval OS_ERR_NULL @p mutex is 0.
 * @retval OS_ERR_IN_ISR Called from exception or interrupt context.
 * @retval OS_ERR_INVALID_ARG A finite timeout is outside the supported
 *         half-range tick limit.
 * @retval OS_ERR_INVALID_STATE No valid current task exists, the idle task is
 *         calling, or the current owner attempts recursive locking.
 * @retval OS_ERR_WOULD_BLOCK The mutex is owned and @p timeout_ticks is
 *         @ref OS_NO_WAIT.
 * @retval OS_ERR_TIMEOUT The finite wait expired before ownership was handed
 *         off.
 *
 * @note The mutex is non-recursive and provides no priority inheritance.
 */
os_status_t os_mutex_lock(os_mutex_t *mutex, uint32_t timeout_ticks);

/**
 * @brief Release a mutex owned by the current task.
 *
 * Ownership is transferred directly to the highest-priority waiter. FIFO
 * order applies among waiters at the same priority. If no task is waiting,
 * the mutex becomes unlocked.
 *
 * @param mutex Initialized mutex to release.
 *
 * @retval OS_OK The mutex was released or handed off successfully.
 * @retval OS_ERR_NULL @p mutex is 0.
 * @retval OS_ERR_IN_ISR Called from exception or interrupt context.
 * @retval OS_ERR_INVALID_STATE No current task exists.
 * @retval OS_ERR_NOT_OWNER The current task does not own @p mutex.
 */
os_status_t os_mutex_unlock(os_mutex_t *mutex);

#endif /* OS_MUTEX_H_ */