#include "app_tasks.h"
#include "os_queue.h"
#include "os_task.h"
#include "os_delay.h"
#include "os_types.h"
#include "stm32l4xx_hal.h"

#define QUEUE_DEPTH 4u

typedef struct {
    uint32_t sender;
    uint32_t sequence;
    uint32_t payload;
    uint32_t checksum;
} test_msg_t;

static os_queue_t queue;
static test_msg_t queue_storage[QUEUE_DEPTH];

volatile uint32_t test_error_count = 0u;

static volatile uint32_t phase = 0u;
static volatile uint32_t receiver_started = 0u;
static volatile uint32_t receiver_done = 0u;
static volatile uint32_t sender_started = 0u;
static volatile uint32_t sender_done = 0u;

enum { PHASE_IDLE = 0u, PHASE_EMPTY_QUEUE = 1u, PHASE_FULL_QUEUE = 2u };

static void test_fail(void) {
    test_error_count++;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
}

static uint32_t msg_checksum(const test_msg_t *msg) {
    return 0xA5A55A5Au ^ msg->sender ^ msg->sequence ^ msg->payload;
}

static test_msg_t make_msg(uint32_t sender, uint32_t sequence, uint32_t payload) {
    test_msg_t msg;

    msg.sender = sender;
    msg.sequence = sequence;
    msg.payload = payload;
    msg.checksum = msg_checksum(&msg);

    return msg;
}

static uint8_t msg_is_equal(const test_msg_t *actual, const test_msg_t *expected) {
    return actual->sender == expected->sender && actual->sequence == expected->sequence &&
           actual->payload == expected->payload && actual->checksum == expected->checksum &&
           actual->checksum == msg_checksum(actual);
}

static void queue_reset(void) {
    if (os_queue_init(&queue, 1, queue_storage, sizeof(test_msg_t), QUEUE_DEPTH) != OS_OK) {
        test_fail();
    }
}

/* Receives one message from an initially empty queue. */
static void receiver_task(void) {
    test_msg_t actual;
    test_msg_t expected;

    while (phase != PHASE_EMPTY_QUEUE) {
        os_delay(1u);
    }

    expected = make_msg(1u, 42u, 0x12345678u);
    receiver_started = 1u;

    if (os_queue_recv(&queue, &actual, OS_WAIT_FOREVER) != OS_OK) {
        test_fail();
    } else if (!msg_is_equal(&actual, &expected)) {
        test_fail();
    }

    receiver_done = 1u;

    while (1) {
        os_delay(100u);
    }
}

/* Sends one message to an initially full queue. */
static void sender_task(void) {
    test_msg_t msg;

    while (phase != PHASE_FULL_QUEUE) {
        os_delay(1u);
    }

    msg = make_msg(2u, QUEUE_DEPTH, 0xCAFEBABEu);
    sender_started = 1u;

    if (os_queue_send(&queue, &msg, OS_WAIT_FOREVER) != OS_OK) {
        test_fail();
    }

    sender_done = 1u;

    while (1) {
        os_delay(100u);
    }
}

static void test_blocking_receive(void) {
    test_msg_t msg;

    queue_reset();
    phase = PHASE_EMPTY_QUEUE;

    while (receiver_started == 0u) {
        os_delay(1u);
    }

    /* The task must still be blocked while the queue is empty. */
    os_delay(10u);
    if (receiver_done != 0u) {
        test_fail();
    }

    msg = make_msg(1u, 42u, 0x12345678u);
    if (os_queue_send(&queue, &msg, OS_NO_WAIT) != OS_OK) {
        test_fail();
    }

    while (receiver_done == 0u) {
        os_delay(1u);
    }

    if (!os_queue_is_empty(&queue)) {
        test_fail();
    }
}

static void test_blocking_send(void) {
    test_msg_t actual;
    test_msg_t expected;

    queue_reset();

    /* Fill the queue with distinguishable messages. */
    for (uint32_t i = 0u; i < QUEUE_DEPTH; i++) {
        expected = make_msg(2u, i, 0x1000u + i);
        if (os_queue_send(&queue, &expected, OS_NO_WAIT) != OS_OK) {
            test_fail();
        }
    }

    if (!os_queue_is_full(&queue)) {
        test_fail();
    }

    phase = PHASE_FULL_QUEUE;

    while (sender_started == 0u) {
        os_delay(1u);
    }

    /* The task must still be blocked while the queue is full. */
    os_delay(10u);
    if (sender_done != 0u) {
        test_fail();
    }

    /* Free one slot and validate the oldest message. */
    expected = make_msg(2u, 0u, 0x1000u);
    if (os_queue_recv(&queue, &actual, OS_NO_WAIT) != OS_OK) {
        test_fail();
    } else if (!msg_is_equal(&actual, &expected)) {
        test_fail();
    }

    while (sender_done == 0u) {
        os_delay(1u);
    }

    /* Validate FIFO order and the message from the unblocked sender. */
    for (uint32_t i = 1u; i <= QUEUE_DEPTH; i++) {
        if (i < QUEUE_DEPTH) {
            expected = make_msg(2u, i, 0x1000u + i);
        } else {
            expected = make_msg(2u, i, 0xCAFEBABEu);
        }

        if (os_queue_recv(&queue, &actual, OS_NO_WAIT) != OS_OK) {
            test_fail();
        } else if (!msg_is_equal(&actual, &expected)) {
            test_fail();
        }
    }

    if (!os_queue_is_empty(&queue)) {
        test_fail();
    }
}

static void monitor_task(void) {
    os_delay(20u);

    test_blocking_receive();
    test_blocking_send();

    if (test_error_count == 0u) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    }

    while (1) {
        os_delay(100u);
    }
}

void app_tasks_init(void) {
    if (os_task_create(receiver_task, 4u) != OS_OK) {
        test_fail();
    }

    if (os_task_create(sender_task, 4u) != OS_OK) {
        test_fail();
    }

    /* Higher numeric value means higher priority. */
    if (os_task_create(monitor_task, 7u) != OS_OK) {
        test_fail();
    }
}