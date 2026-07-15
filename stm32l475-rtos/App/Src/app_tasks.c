#include "app_tasks.h"

#include "os_delay.h"
#include "os_queue.h"
#include "os_task.h"
#include "os_types.h"

#include "stm32l4xx_hal.h"

#include <stdint.h>

#define QUEUE_ID        1u
#define QUEUE_DEPTH     2u
#define MESSAGE_COUNT   6u
#define RECEIVE_TIMEOUT 5u

#define PRODUCER_PRIORITY 3u
#define CONSUMER_PRIORITY 3u
#define MONITOR_PRIORITY  7u

typedef struct {
    uint32_t sequence;
    uint32_t value;
    uint32_t checksum;
} queue_message_t;

static os_queue_t queue;
static queue_message_t queue_storage[QUEUE_DEPTH];

static volatile uint32_t producer_started;
static volatile uint32_t producer_finished;
static volatile uint32_t consumer_started;
static volatile uint32_t consumer_finished;
static volatile uint32_t consumed_count;
static volatile uint32_t test_errors;

static uint32_t message_checksum(uint32_t sequence, uint32_t value) {
    return 0xA5A50000u ^ sequence ^ value;
}

static queue_message_t make_message(uint32_t sequence) {
    queue_message_t message;

    message.sequence = sequence;
    message.value = 1000u + sequence;
    message.checksum = message_checksum(message.sequence, message.value);

    return message;
}

static uint8_t message_is_valid(const queue_message_t *message, uint32_t expected_sequence) {
    if (message == 0) {
        return 0u;
    }

    if (message->sequence != expected_sequence) {
        return 0u;
    }

    if (message->value != (1000u + expected_sequence)) {
        return 0u;
    }

    if (message->checksum != message_checksum(message->sequence, message->value)) {
        return 0u;
    }

    return 1u;
}

static void test_fail(void) {
    test_errors++;

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
}

static void producer_task(void) {
    producer_started = 1u;

    for (uint32_t sequence = 0u; sequence < MESSAGE_COUNT; ++sequence) {
        queue_message_t message = make_message(sequence);

        if (os_queue_send(&queue, &message, OS_WAIT_FOREVER) != OS_OK) {
            test_fail();
        }
    }

    producer_finished = 1u;

    while (1) {
        os_delay(100u);
    }
}

static void consumer_task(void) {
    queue_message_t message;

    consumer_started = 1u;

    /*
     * Allow the producer to fill the queue and block.
     *
     * With QUEUE_DEPTH = 2 and MESSAGE_COUNT = 6, the producer must
     * block unless the queue implementation incorrectly overwrites data.
     */
    os_delay(10u);

    for (uint32_t expected = 0u; expected < MESSAGE_COUNT; ++expected) {
        if (os_queue_recv(&queue, &message, OS_WAIT_FOREVER) != OS_OK) {
            test_fail();
            continue;
        }

        if (!message_is_valid(&message, expected)) {
            test_fail();
        }

        consumed_count++;
    }

    /*
     * The queue must now be empty. A timed receive must block and finish
     * with OS_ERR_TIMEOUT.
     */
    if (os_queue_recv(&queue, &message, RECEIVE_TIMEOUT) != OS_ERR_TIMEOUT) {
        test_fail();
    }

    consumer_finished = 1u;

    while (1) {
        os_delay(100u);
    }
}

static void monitor_task(void) {
    queue_message_t message;
    queue_message_t output;

    /*
     * Basic empty/full checks before concurrent execution.
     */
    if (!os_queue_is_empty(&queue)) {
        test_fail();
    }

    if (os_queue_is_full(&queue)) {
        test_fail();
    }

    if (os_queue_recv(&queue, &output, OS_NO_WAIT) != OS_ERR_WOULD_BLOCK) {
        test_fail();
    }

    for (uint32_t index = 0u; index < QUEUE_DEPTH; ++index) {
        message = make_message(100u + index);

        if (os_queue_send(&queue, &message, OS_NO_WAIT) != OS_OK) {
            test_fail();
        }
    }

    if (!os_queue_is_full(&queue)) {
        test_fail();
    }

    message = make_message(999u);

    if (os_queue_send(&queue, &message, OS_NO_WAIT) != OS_ERR_WOULD_BLOCK) {
        test_fail();
    }

    for (uint32_t index = 0u; index < QUEUE_DEPTH; ++index) {
        if (os_queue_recv(&queue, &output, OS_NO_WAIT) != OS_OK) {
            test_fail();
            continue;
        }

        if (!message_is_valid(&output, 100u + index)) {
            test_fail();
        }
    }

    /*
     * Let producer and consumer run.
     */
    while ((producer_started == 0u) || (consumer_started == 0u)) {
        os_delay(1u);
    }

    while ((producer_finished == 0u) || (consumer_finished == 0u)) {
        os_delay(1u);
    }

    if (consumed_count != MESSAGE_COUNT) {
        test_fail();
    }

    if (!os_queue_is_empty(&queue)) {
        test_fail();
    }

    if (test_errors == 0u) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    }

    while (1) {
        os_delay(100u);
    }
}

void app_tasks_init(void) {
    producer_started = 0u;
    producer_finished = 0u;
    consumer_started = 0u;
    consumer_finished = 0u;
    consumed_count = 0u;
    test_errors = 0u;

    if (os_queue_init(&queue, QUEUE_ID, queue_storage, sizeof(queue_message_t), QUEUE_DEPTH) !=
        OS_OK) {
        test_fail();
        return;
    }

    if (os_task_create(producer_task, PRODUCER_PRIORITY) != OS_OK) {
        test_fail();
    }

    if (os_task_create(consumer_task, CONSUMER_PRIORITY) != OS_OK) {
        test_fail();
    }

    if (os_task_create(monitor_task, MONITOR_PRIORITY) != OS_OK) {
        test_fail();
    }
}