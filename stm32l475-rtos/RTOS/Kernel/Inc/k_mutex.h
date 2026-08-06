/**
 * @file k_mutex.h
 * @brief Internal mutex timeout cleanup.
 * @author Jerome
 *
 * @details
 * Declares the cleanup hook used by the timeout subsystem when a finite
 * @c os_mutex_lock() wait expires.
 */

#ifndef K_MUTEX_H_
#define K_MUTEX_H_

#include "os_mutex.h"
#include "kernel_task.h"

/**
 * @brief Remove a timed-out task from a mutex wait queue.
 *
 * Removes @p task from @p mutex's priority wait list, clears its mutex-wait
 * metadata, and sets its wait result to @c OS_ERR_TIMEOUT. The timeout
 * subsystem removes the task's timeout entry before this call and readies the
 * task afterward.
 *
 * @param mutex Mutex on which the task was blocked.
 * @param task Task whose finite mutex wait expired.
 *
 * @pre @p mutex must not be null.
 * @pre @p task must not be null.
 * @pre @c task->wait_type must equal @c K_WAIT_MUTEX.
 * @pre @c task->wait_object must equal @p mutex.
 * @pre @p task must be linked in @c mutex->wait_list.
 * @pre The caller must hold the kernel's required synchronization.
 * @post The task is removed from the mutex wait list.
 * @post The task has no active wait object and reports @c OS_ERR_TIMEOUT.
 *
 * @note This function does not change mutex ownership or ready the task.
 */
void k_mutex_timeout_cleanup(os_mutex_t *mutex, kernel_task_t *task);

#endif /* K_MUTEX_H_ */
