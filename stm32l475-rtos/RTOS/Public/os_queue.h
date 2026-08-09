/**
 * @file os_queue.h
 * @brief Fixed-capacity message queues.
 * @author Jerome
 * @author Martin
 *
 * Provides caller-backed queues for fixed-size messages. Buffered messages are
 * FIFO ordered, while blocked senders and receivers are selected by task
 * priority and FIFO order within the same priority.
 */
#ifndef OS_QUEUE_H_
#define OS_QUEUE_H_

#include "kernel_task.h"
#include "os_types.h"
#include "prio_waitq.h"
#include "ring_msgbuf.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Message-queue object.
 *
 * Initialize with os_queue_init() before use. Members are internal queue state
 * and must not be modified directly after initialization.
 */
typedef struct {
    uint32_t id;                 ///< Unique queue ID for trace events.
    ring_msgbuf_t buffer;        ///< Fixed-size message ring buffer.
    prio_waitq_t send_wait_list; ///< Tasks blocked while sending.
    prio_waitq_t recv_wait_list; ///< Tasks blocked while receiving.
} os_queue_t;

/**
 * @brief Initialize a fixed-capacity message queue.
 *
 * @param queue Queue object to initialize.
 * @param id Unique queue ID for trace events.
 * @param storage Caller-owned message storage.
 * @param msg_size Size of each message in bytes.
 * @param msg_count Number of messages that fit in @p storage.
 *
 * @retval OS_OK Queue initialized successfully.
 * @retval OS_ERR_NULL @p queue or @p storage is 0.
 * @retval OS_ERR_INVALID_ARG @p msg_size or @p msg_count is 0.
 *
 * @pre @p storage must reference at least <tt>msg_size * msg_count</tt>
 *      writable bytes with suitable lifetime.
 * @note The storage must remain valid and must not be modified externally
 *       while the queue is in use.
 */
os_status_t os_queue_init(
    os_queue_t *queue, uint32_t id, void *storage, uint32_t msg_size, uint32_t msg_count);

/**
 * @brief Send one fixed-size message to a queue.
 *
 * If a receiver is waiting, the message is copied directly to that receiver.
 * Otherwise, it is appended to the FIFO ring buffer. When neither path can
 * complete immediately, the caller may wait according to @p timeout_ticks.
 *
 * @param queue Initialized destination queue.
 * @param msg Message data; exactly the configured message size is copied.
 * @param timeout_ticks Wait policy: @ref OS_NO_WAIT, @ref OS_WAIT_FOREVER, or
 *        a valid nonzero finite timeout in system ticks.
 *
 * @retval OS_OK The message was buffered or delivered to a receiver.
 * @retval OS_ERR_NULL @p queue or @p msg is 0.
 * @retval OS_ERR_INVALID_ARG A finite timeout is outside the supported
 *         half-range tick limit.
 * @retval OS_ERR_WOULD_BLOCK The queue cannot accept the message immediately
 *         and @p timeout_ticks is @ref OS_NO_WAIT.
 * @retval OS_ERR_IN_ISR The operation would block in exception context.
 * @retval OS_ERR_INVALID_STATE No valid current task exists or the caller is
 *         the idle task when blocking is required.
 * @retval OS_ERR_TIMEOUT The finite wait expired before the message was sent.
 *
 * @note Exception-context calls are permitted only when the send completes
 *       immediately without blocking.
 * @warning When blocking, @p msg must remain valid and unmodified until this
 *          function returns.
 */
os_status_t os_queue_send(os_queue_t *queue, const void *msg, uint32_t timeout_ticks);

/**
 * @brief Receive one fixed-size message from a queue.
 *
 * Buffered messages are consumed first to preserve FIFO order. If the buffer
 * is empty but a sender is waiting, its message is copied directly. Otherwise,
 * the caller may wait according to @p timeout_ticks.
 *
 * @param queue Initialized source queue.
 * @param[out] msg_out Destination receiving exactly one configured-size
 *        message.
 * @param timeout_ticks Wait policy: @ref OS_NO_WAIT, @ref OS_WAIT_FOREVER, or
 *        a valid nonzero finite timeout in system ticks.
 *
 * @retval OS_OK A message was copied to @p msg_out.
 * @retval OS_ERR_NULL @p queue or @p msg_out is 0.
 * @retval OS_ERR_INVALID_ARG A finite timeout is outside the supported
 *         half-range tick limit.
 * @retval OS_ERR_WOULD_BLOCK No message is available immediately and
 *         @p timeout_ticks is @ref OS_NO_WAIT.
 * @retval OS_ERR_IN_ISR The operation would block in exception context.
 * @retval OS_ERR_INVALID_STATE No valid current task exists or the caller is
 *         the idle task when blocking is required.
 * @retval OS_ERR_TIMEOUT The finite wait expired before a message arrived.
 *
 * @note Exception-context calls are permitted only when the receive completes
 *       immediately without blocking.
 * @warning When blocking, @p msg_out must remain valid until this function
 *          returns.
 */
os_status_t os_queue_recv(os_queue_t *queue, void *msg_out, uint32_t timeout_ticks);

/**
 * @brief Test whether a queue contains no buffered messages.
 *
 * Blocked direct-handoff senders are not counted as buffered messages.
 *
 * @param queue Queue to inspect.
 * @retval true @p queue is 0 or its ring buffer is empty.
 * @retval false At least one message is buffered.
 */
bool os_queue_is_empty(os_queue_t *queue);

/**
 * @brief Test whether a queue's ring buffer is full.
 *
 * @param queue Queue to inspect.
 * @retval true The ring buffer is at capacity.
 * @retval false @p queue is 0 or at least one buffer slot is free.
 */
bool os_queue_is_full(os_queue_t *queue);

#endif /* OS_QUEUE_H_ */