/**
 * @file k_sem.h
 * @brief Internal semaphore timeout cleanup.
 * @author Jerome
 *
 * Declares the cleanup hook used when a task's timed semaphore wait expires.
 */
#ifndef K_SEM_H_
#define K_SEM_H_

#include "kernel_task.h"
#include "os_sem.h"
#include "task_list.h"

/**
 * @brief Clean up a task whose semaphore wait has timed out.
 *
 * Removes @p task from @p sem's priority wait queue, clears the task's wait
 * association, and stores @ref OS_ERR_TIMEOUT as its wait result. The timeout
 * subsystem is responsible for making the task ready afterward.
 *
 * @param sem Semaphore on which the task was waiting.
 * @param task Task whose wait timed out.
 *
 * @pre @p sem and @p task must not be 0.
 * @pre @p task must be waiting on @p sem with wait type @ref K_WAIT_SEM.
 * @pre @p task must be linked in the semaphore's wait queue.
 * @pre The caller must provide the required kernel synchronization.
 *
 * @post The task is removed from the semaphore wait queue.
 * @post The task has no active wait object and its result is
 *       @ref OS_ERR_TIMEOUT.
 *
 * @note This function does not modify the semaphore count or ready the task.
 */
void k_sem_timeout_cleanup(os_sem_t *sem, kernel_task_t *task);

#endif /* K_SEM_H_ */