/**
 * @file integration_queue.c
 * @brief Queue communication, integrity, FIFO, and blocking integration test.
 * @author Jerome
 *
 * @details
 * A one-element queue forces both empty-receive and full-send blocking paths.
 * The test covers direct task-to-task delivery, buffered FIFO delivery, sender
 * and receiver wake-up, and byte-preserving transport of 32-bit test messages.
 * Redundant message fields detect corruption without assuming values wider
 * than the target's native 32-bit data path. Existing queue trace events carry
 * a 32-bit message hash; the C checks still compare every message field.
 *
 * @par Test sequence
 * 1. A high-priority receiver blocks on the empty queue.
 * 2. The producer sends one message by direct handoff to that receiver.
 * 3. The producer fills the queue, observes no-wait failure, then blocks while
 *    sending that same second message.
 * 4. The lower-priority drainer receives the first buffered message.
 * 5. The freed slot accepts the blocked send and wakes the producer.
 * 6. The drainer receives the second message and verifies FIFO and integrity.
 * 7. A non-blocking read verifies rejection on the empty queue.
 */

#include "integration_test.h"
#include "integration_tests.h"
#include "project.h"

#if PROJECT == PROJECT_QUEUE

#include "os_delay.h"
#include "os_queue.h"
#include "os_task.h"
#include "os_types.h"

#include <stdbool.h>
#include <stdint.h>

#define QUEUE_TEST_ID           (1u)          /**< Queue identifier used by the test. */
#define QUEUE_CAPACITY          (1u)          /**< Capacity chosen to force full blocking. */
#define QUEUE_RECEIVER_PRIORITY (6u)          /**< Empty-queue receiver priority. */
#define QUEUE_PRODUCER_PRIORITY (5u)          /**< Message producer priority. */
#define QUEUE_DRAINER_PRIORITY  (4u)          /**< Buffered-message consumer priority. */
#define QUEUE_PARK_TICKS        (1000000u)    /**< Long delay after task completion. */
#define QUEUE_MESSAGE_MAGIC     (0x51A7E123u) /**< Salt used by the integrity checksum. */

/** @brief Message with redundant fields used for integrity checks. */
typedef struct {
    uint32_t sequence; /**< Monotonic value used to verify FIFO order. */
    uint32_t payload;  /**< Application data that must remain unchanged. */
    uint32_t inverse;  /**< Bitwise inverse of @c payload. */
    uint32_t checksum; /**< XOR checksum of all preceding logical fields. */
} queue_test_message_t;

/** @brief Progress state of a blocking queue operation. */
typedef enum {
    QUEUE_STAGE_NOT_STARTED = 0u, /**< Participant has not begun its operation. */
    QUEUE_STAGE_WAITING,          /**< Participant is entering a blocking call. */
    QUEUE_STAGE_COMPLETE          /**< Blocking operation returned successfully. */
} queue_test_stage_t;

/** @brief Debugger-visible queue-test observations. */
typedef struct {
    volatile queue_test_stage_t empty_receiver_stage; /**< Empty receiver progress. */
    volatile queue_test_stage_t full_sender_stage;    /**< Full-queue sender progress. */
    volatile uint32_t direct_handoff_valid;           /**< Direct message passed integrity. */
    volatile uint32_t buffered_messages_valid;        /**< Buffered FIFO messages passed. */
} queue_test_observation_t;

/** @brief Live queue observations for debugger inspection. */
queue_test_observation_t g_queue_test_observation;

/** @brief Queue object shared by producer and receivers. */
static os_queue_t g_test_queue;

/** @brief Single-slot, correctly typed backing storage for the queue. */
static queue_test_message_t g_queue_storage[QUEUE_CAPACITY];

/**
 * @brief Build a deterministic message with internal integrity redundancy.
 * @param sequence FIFO sequence number assigned to the message.
 * @param payload 32-bit payload transported by the queue.
 * @return Fully initialized message including inverse and checksum fields.
 */
static queue_test_message_t queue_make_message(uint32_t sequence, uint32_t payload) {
    queue_test_message_t message;

    message.sequence = sequence;
    message.payload = payload;
    message.inverse = ~payload;
    message.checksum = sequence ^ payload ^ message.inverse ^ QUEUE_MESSAGE_MAGIC;

    return message;
}

/**
 * @brief Check exact equality and the internal integrity relationships.
 * @param actual Message received from the queue.
 * @param expected Original message supplied by the sender.
 * @return @c true when every field is unchanged and both redundancy checks
 *         are internally consistent; otherwise @c false.
 * @pre @p actual and @p expected point to valid messages.
 */
static bool queue_message_matches(const queue_test_message_t *actual,
                                  const queue_test_message_t *expected) {
    uint32_t checksum = actual->sequence ^ actual->payload ^ actual->inverse ^ QUEUE_MESSAGE_MAGIC;

    return (actual->sequence == expected->sequence) && (actual->payload == expected->payload) &&
           (actual->inverse == expected->inverse) && (actual->checksum == expected->checksum) &&
           (actual->inverse == ~actual->payload) && (actual->checksum == checksum);
}

/**
 * @brief Keep a completed queue participant out of the ready set.
 * @note This function does not return.
 */
static void queue_park(void) {
    while (1) {
        integration_test_check(os_delay(QUEUE_PARK_TICKS) == OS_OK);
    }
}

/**
 * @brief Exercise blocking receive and direct producer-to-receiver handoff.
 * @post The received message is validated before @c direct_handoff_valid is set.
 * @post @c empty_receiver_stage becomes @ref QUEUE_STAGE_COMPLETE.
 */
static void queue_empty_receiver_task(void) {
    queue_test_message_t received;
    queue_test_message_t expected = queue_make_message(1u, 0x11223344u);

    g_queue_test_observation.empty_receiver_stage = QUEUE_STAGE_WAITING;
    integration_test_check(os_queue_recv(&g_test_queue, &received, OS_WAIT_FOREVER) == OS_OK);
    integration_test_check(queue_message_matches(&received, &expected));

    g_queue_test_observation.direct_handoff_valid = 1u;
    g_queue_test_observation.empty_receiver_stage = QUEUE_STAGE_COMPLETE;
    queue_park();
}

/**
 * @brief Perform direct handoff and then exercise a blocking full-queue send.
 *
 * The producer first wakes the blocked receiver, fills the one-slot queue, and
 * then attempts a second infinite-wait send. That final call can return only
 * after the drainer creates space and the queue accepts the pending message.
 *
 * @post @c full_sender_stage becomes @ref QUEUE_STAGE_COMPLETE after wake-up.
 */
static void queue_producer_task(void) {
    queue_test_message_t direct = queue_make_message(1u, 0x11223344u);
    queue_test_message_t first = queue_make_message(2u, 0x55667788u);
    queue_test_message_t second = queue_make_message(3u, 0xA5A55A5Au);

    integration_test_check(g_queue_test_observation.empty_receiver_stage == QUEUE_STAGE_WAITING);
    integration_test_check(os_queue_send(&g_test_queue, &direct, OS_NO_WAIT) == OS_OK);
    integration_test_check(g_queue_test_observation.empty_receiver_stage == QUEUE_STAGE_COMPLETE);

    integration_test_check(os_queue_send(&g_test_queue, &first, OS_NO_WAIT) == OS_OK);
    integration_test_check(os_queue_is_full(&g_test_queue));

    /* Full queue: first prove no-wait rejection, then block on the same data. */
    integration_test_check(os_queue_send(&g_test_queue, &second, OS_NO_WAIT) ==
                           OS_ERR_WOULD_BLOCK);
    g_queue_test_observation.full_sender_stage = QUEUE_STAGE_WAITING;
    integration_test_check(os_queue_send(&g_test_queue, &second, OS_WAIT_FOREVER) == OS_OK);
    g_queue_test_observation.full_sender_stage = QUEUE_STAGE_COMPLETE;

    queue_park();
}

/**
 * @brief Drain the queue and verify FIFO, integrity, and blocked-sender wake-up.
 *
 * This lower-priority task becomes runnable only after the producer blocks on
 * a full queue. Receiving the first item must cause the pending second send to
 * complete, after which the second item is received in FIFO order.
 *
 * @post The aggregate result is passed after both message paths and empty-read
 *       rejection have been validated.
 */
static void queue_drainer_task(void) {
    queue_test_message_t received;
    queue_test_message_t first = queue_make_message(2u, 0x55667788u);
    queue_test_message_t second = queue_make_message(3u, 0xA5A55A5Au);

    /* This task runs only after the higher-priority sender has blocked. */
    integration_test_check(g_queue_test_observation.full_sender_stage == QUEUE_STAGE_WAITING);
    integration_test_check(os_queue_is_full(&g_test_queue));

    integration_test_check(os_queue_recv(&g_test_queue, &received, OS_WAIT_FOREVER) == OS_OK);
    integration_test_check(queue_message_matches(&received, &first));

    /* Receiving the first item must free/refill a slot and wake the sender. */
    integration_test_check(g_queue_test_observation.full_sender_stage == QUEUE_STAGE_COMPLETE);

    integration_test_check(os_queue_recv(&g_test_queue, &received, OS_NO_WAIT) == OS_OK);
    integration_test_check(queue_message_matches(&received, &second));
    integration_test_check(os_queue_is_empty(&g_test_queue));
    integration_test_check(os_queue_recv(&g_test_queue, &received, OS_NO_WAIT) ==
                           OS_ERR_WOULD_BLOCK);

    g_queue_test_observation.buffered_messages_valid = 1u;
    integration_test_check(g_queue_test_observation.direct_handoff_valid != 0u);
    integration_test_pass();
    queue_park();
}

/** @copydoc integration_queue_init */
void integration_queue_init(void) {
    g_queue_test_observation.empty_receiver_stage = QUEUE_STAGE_NOT_STARTED;
    g_queue_test_observation.full_sender_stage = QUEUE_STAGE_NOT_STARTED;
    g_queue_test_observation.direct_handoff_valid = 0u;
    g_queue_test_observation.buffered_messages_valid = 0u;

    integration_test_check(os_queue_init(&g_test_queue,
                                         QUEUE_TEST_ID,
                                         g_queue_storage,
                                         sizeof(queue_test_message_t),
                                         QUEUE_CAPACITY) == OS_OK);
    integration_test_check(os_task_create(queue_empty_receiver_task, QUEUE_RECEIVER_PRIORITY) ==
                           OS_OK);
    integration_test_check(os_task_create(queue_producer_task, QUEUE_PRODUCER_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(queue_drainer_task, QUEUE_DRAINER_PRIORITY) == OS_OK);
}

#endif /* PROJECT == PROJECT_QUEUE */
