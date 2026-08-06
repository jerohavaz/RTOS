/**
 * @file ring_msgbuf.h
 * @brief Internal fixed-message-size circular buffer.
 * @author Jerome
 * @author Martin
 *
 * @details
 * Stores messages by value in caller-provided memory. Messages are inserted
 * and removed in FIFO order. The implementation performs no allocation and
 * does not own the backing storage.
 *
 * All operations have constant time complexity. The buffer does not provide
 * internal synchronization; callers must serialize concurrent access.
 */

#ifndef RING_MSGBUF_H_
#define RING_MSGBUF_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief State of a fixed-size-message ring buffer.
 *
 * The backing memory is divided into @ref capacity slots of @ref msg_size
 * bytes. @ref head identifies the next slot to read, @ref tail identifies the
 * next slot to write, and @ref count distinguishes an empty ring from a full
 * ring when both indices are equal.
 *
 * @invariant @c count <= capacity.
 * @invariant @c head < capacity and @c tail < capacity after initialization.
 */
typedef struct {
    uint8_t *buffer;   /**< Caller-owned backing storage. */
    uint32_t msg_size; /**< Size of every message, in bytes. */
    uint32_t capacity; /**< Number of message slots in the backing storage. */
    uint32_t count;    /**< Number of messages currently stored. */
    uint32_t head;     /**< Index of the next slot read by @ref ring_msgbuf_pop. */
    uint32_t tail;     /**< Index of the next slot written by @ref ring_msgbuf_push. */
} ring_msgbuf_t;

/**
 * @brief Initialize an empty message ring.
 *
 * Associates the ring with external storage and resets its count, read index,
 * and write index to zero. Existing bytes in the storage are not cleared.
 *
 * @param rb Ring object to initialize.
 * @param buffer Backing storage containing at least
 *               @p msg_size * @p capacity bytes.
 * @param msg_size Size of one message, in bytes.
 * @param capacity Number of messages the ring can hold.
 *
 * @pre @p rb must not be null.
 * @pre @p buffer must not be null.
 * @pre @p msg_size must be greater than zero.
 * @pre @p capacity must be greater than zero.
 * @pre The product @p msg_size * @p capacity must be representable by the
 *      target address arithmetic and must not exceed the supplied object.
 * @post The ring is empty.
 *
 * @note The backing storage must remain valid and writable for the entire
 *       lifetime of the initialized ring.
 */
void ring_msgbuf_init(ring_msgbuf_t *rb, void *buffer, uint32_t msg_size, uint32_t capacity);

/**
 * @brief Test whether the ring contains no messages.
 *
 * @param rb Initialized ring to inspect.
 *
 * @retval true The stored-message count is zero.
 * @retval false At least one message is stored.
 *
 * @pre @p rb must not be null.
 */
bool ring_msgbuf_is_empty(const ring_msgbuf_t *rb);

/**
 * @brief Test whether the ring has no free message slot.
 *
 * @param rb Initialized ring to inspect.
 *
 * @retval true The stored-message count is at least the configured capacity.
 * @retval false At least one message slot is available.
 *
 * @pre @p rb must not be null.
 */
bool ring_msgbuf_is_full(const ring_msgbuf_t *rb);

/**
 * @brief Return the number of currently stored messages.
 *
 * @param rb Initialized ring to inspect.
 *
 * @return Stored-message count in the inclusive range 0 through capacity.
 *
 * @pre @p rb must not be null.
 */
uint32_t ring_msgbuf_count(const ring_msgbuf_t *rb);

/**
 * @brief Return the configured number of message slots.
 *
 * @param rb Initialized ring to inspect.
 *
 * @return Message capacity supplied to @ref ring_msgbuf_init.
 *
 * @pre @p rb must not be null.
 */
uint32_t ring_msgbuf_capacity(const ring_msgbuf_t *rb);

/**
 * @brief Return the configured message size.
 *
 * @param rb Initialized ring to inspect.
 *
 * @return Size of one message in bytes.
 *
 * @pre @p rb must not be null.
 */
uint32_t ring_msgbuf_msg_size(const ring_msgbuf_t *rb);

/**
 * @brief Copy one message into the tail of the ring.
 *
 * Copies exactly @ref ring_msgbuf_t::msg_size bytes from @p msg into the next
 * writable slot, advances and wraps the tail index, and increments the count.
 *
 * @param rb Initialized ring with at least one free slot.
 * @param msg Source object containing at least
 *            @ref ring_msgbuf_t::msg_size readable bytes.
 *
 * @pre @p rb must not be null.
 * @pre @p msg must not be null.
 * @pre The ring must not be full.
 * @pre The source range must not overlap the destination ring slot because the
 *      implementation uses @c memcpy.
 */
void ring_msgbuf_push(ring_msgbuf_t *rb, const void *msg);

/**
 * @brief Remove the oldest message from the head of the ring.
 *
 * Copies exactly @ref ring_msgbuf_t::msg_size bytes from the oldest occupied
 * slot into @p msg_out, advances and wraps the head index, and decrements the
 * count. Removed bytes are not cleared from the backing storage.
 *
 * @param rb Initialized ring containing at least one message.
 * @param msg_out Destination object with space for at least
 *                @ref ring_msgbuf_t::msg_size writable bytes.
 *
 * @pre @p rb must not be null.
 * @pre @p msg_out must not be null.
 * @pre The ring must not be empty.
 * @pre The destination range must not overlap the source ring slot because the
 *      implementation uses @c memcpy.
 */
void ring_msgbuf_pop(ring_msgbuf_t *rb, void *msg_out);

#endif /* RING_MSGBUF_H_ */