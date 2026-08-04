/**
 * @file k_queue.h
 * @brief Internal message-queue timeout cleanup.
 * @author Jerome
 * @author Martin
 *
 * @details
 * Declares the cleanup hooks used when finite queue send or receive waits
 * expire.
 */

#ifndef K_QUEUE_H_
#define K_QUEUE_H_

#include "kernel_task.h"
#include "os_queue.h"

/**
 * @brief Remove a timed-out sender from a queue.
 *
 * Removes @p task from the send wait list, clears its queued source-message
 * pointer and wait metadata, and sets its result to @c OS_ERR_TIMEOUT.
 *
 * @param queue Queue on which the task was waiting to send.
 * @param task Sending task whose finite wait expired.
 *
 * @pre Both arguments must be non-null.
 * @pre @c task->wait_type must equal @c K_WAIT_QUEUE_SEND.
 * @pre @c task->wait_object must equal @p queue.
 * @pre @p task must be linked in @c queue->send_wait_list.
 * @pre The caller must hold the kernel's required synchronization.
 * @post The task has no active wait object or wait-data pointer and reports
 *       @c OS_ERR_TIMEOUT.
 *
 * @note This function does not ready the task.
 */
void k_queue_send_timeout_cleanup(os_queue_t *queue, kernel_task_t *task);

/**
 * @brief Remove a timed-out receiver from a queue.
 *
 * Removes @p task from the receive wait list, clears its destination-message
 * pointer and wait metadata, and sets its result to @c OS_ERR_TIMEOUT.
 *
 * @param queue Queue from which the task was waiting to receive.
 * @param task Receiving task whose finite wait expired.
 *
 * @pre Both arguments must be non-null.
 * @pre @c task->wait_type must equal @c K_WAIT_QUEUE_RECV.
 * @pre @c task->wait_object must equal @p queue.
 * @pre @p task must be linked in @c queue->recv_wait_list.
 * @pre The caller must hold the kernel's required synchronization.
 * @post The task has no active wait object or wait-data pointer and reports
 *       @c OS_ERR_TIMEOUT.
 *
 * @note This function does not ready the task.
 */
void k_queue_recv_timeout_cleanup(os_queue_t *queue, kernel_task_t *task);

#endif /* K_QUEUE_H_ */