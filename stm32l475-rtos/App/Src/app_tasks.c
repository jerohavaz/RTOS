#include "app_tasks.h"
#include "os_queue.h"
#include "os_task.h"
#include "os_delay.h"
#include "os_types.h"
#include "stm32l4xx_hal.h"

#define QUEUE_DEPTH           8u
#define PRODUCER_COUNT        2u
#define CONSUMER_COUNT        2u
#define MESSAGES_PER_PRODUCER 1000u
#define TIMEOUT_TICKS         5u

#define TEST_MSG_RECV_HANDOFF     0x11111111u
#define TEST_MSG_BLOCKED_SEND_OLD 0x22222222u
#define TEST_MSG_BLOCKED_SEND_NEW 0x33333333u
#define TEST_MSG_PRIO_FIRST       0x44444444u
#define TEST_MSG_PRIO_SECOND      0x55555555u

typedef struct {
    uint32_t producer_id;
    uint32_t sequence;
    uint32_t checksum;
} queue_test_msg_t;

static os_queue_t queue;
static queue_test_msg_t queue_storage[QUEUE_DEPTH];

volatile uint32_t test_error_count = 0u;
volatile uint32_t test_phase = 0u;

volatile uint32_t produced_count = 0u;
volatile uint32_t consumed_count = 0u;
volatile uint32_t producer_done_count = 0u;
volatile uint32_t consumer_done_count = 0u;

volatile uint32_t recv_handoff_waiting = 0u;
volatile uint32_t recv_handoff_done = 0u;

volatile uint32_t blocked_sender_started = 0u;
volatile uint32_t blocked_sender_done = 0u;

volatile uint32_t recv_timeout_started = 0u;
volatile uint32_t recv_timeout_done = 0u;

volatile uint32_t send_timeout_started = 0u;
volatile uint32_t send_timeout_done = 0u;

volatile uint32_t prio_low_waiting = 0u;
volatile uint32_t prio_high_waiting = 0u;
volatile uint32_t prio_low_done = 0u;
volatile uint32_t prio_high_done = 0u;
volatile uint32_t prio_low_value = 0u;
volatile uint32_t prio_high_value = 0u;

static volatile uint32_t producer_next_id = 0u;
static volatile uint32_t consumer_next_id = 0u;

static uint8_t seen[PRODUCER_COUNT][MESSAGES_PER_PRODUCER];

enum {
    PHASE_IDLE = 0u,
    PHASE_RECV_HANDOFF = 1u,
    PHASE_BLOCKED_SEND = 2u,
    PHASE_RECV_TIMEOUT = 3u,
    PHASE_SEND_TIMEOUT = 4u,
    PHASE_PRIO_RECV = 5u,
    PHASE_STRESS = 6u
};

static void test_fail(void) {
    test_error_count++;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
}

static void wait_for_phase(uint32_t phase) {
    while (test_phase != phase) {
        os_delay(10u);
    }
}

static uint32_t make_checksum(uint32_t producer_id, uint32_t sequence) {
    return 0xA5A50000u ^ (producer_id << 16) ^ sequence;
}

static void reset_queue(void) {
    if (os_queue_init(&queue, queue_storage, sizeof(queue_test_msg_t), QUEUE_DEPTH) != OS_OK) {
        test_fail();
    }
}

static queue_test_msg_t make_raw_msg(uint32_t value) {
    queue_test_msg_t msg;

    msg.producer_id = 0xABCD0000u;
    msg.sequence = value;
    msg.checksum = 0x12345678u ^ value;

    return msg;
}

static uint8_t is_raw_msg(const queue_test_msg_t *msg, uint32_t value) {
    if (msg->producer_id != 0xABCD0000u) {
        return 0u;
    }

    if (msg->sequence != value) {
        return 0u;
    }

    if (msg->checksum != (0x12345678u ^ value)) {
        return 0u;
    }

    return 1u;
}

static void basic_queue_tests(void) {
    queue_test_msg_t msg;
    queue_test_msg_t out;

    reset_queue();

    if (!os_queue_is_empty(&queue)) {
        test_fail();
    }

    if (os_queue_is_full(&queue)) {
        test_fail();
    }

    if (os_queue_recv(&queue, &out, OS_NO_WAIT) != OS_ERR_WOULD_BLOCK) {
        test_fail();
    }

    for (uint32_t i = 0u; i < QUEUE_DEPTH; i++) {
        msg.producer_id = 99u;
        msg.sequence = i;
        msg.checksum = make_checksum(msg.producer_id, msg.sequence);

        if (os_queue_send(&queue, &msg, OS_NO_WAIT) != OS_OK) {
            test_fail();
        }
    }

    if (!os_queue_is_full(&queue)) {
        test_fail();
    }

    msg.producer_id = 99u;
    msg.sequence = 0xFFFFFFFFu;
    msg.checksum = make_checksum(msg.producer_id, msg.sequence);

    if (os_queue_send(&queue, &msg, OS_NO_WAIT) != OS_ERR_WOULD_BLOCK) {
        test_fail();
    }

    for (uint32_t i = 0u; i < QUEUE_DEPTH; i++) {
        if (os_queue_recv(&queue, &out, OS_NO_WAIT) != OS_OK) {
            test_fail();
        }

        if (out.producer_id != 99u) {
            test_fail();
        }

        if (out.sequence != i) {
            test_fail();
        }

        if (out.checksum != make_checksum(out.producer_id, out.sequence)) {
            test_fail();
        }
    }

    if (!os_queue_is_empty(&queue)) {
        test_fail();
    }
}

static void recv_handoff_task(void) {
    queue_test_msg_t out;
    os_status_t status;

    wait_for_phase(PHASE_RECV_HANDOFF);

    recv_handoff_waiting = 1u;

    status = os_queue_recv(&queue, &out, OS_WAIT_FOREVER);

    if (status != OS_OK) {
        test_fail();
    }

    if (!is_raw_msg(&out, TEST_MSG_RECV_HANDOFF)) {
        test_fail();
    }

    recv_handoff_done = 1u;

    while (1) {
        os_delay(100u);
    }
}

static void blocked_sender_task(void) {
    queue_test_msg_t msg;
    os_status_t status;

    wait_for_phase(PHASE_BLOCKED_SEND);

    msg = make_raw_msg(TEST_MSG_BLOCKED_SEND_NEW);

    blocked_sender_started = 1u;

    status = os_queue_send(&queue, &msg, OS_WAIT_FOREVER);

    if (status != OS_OK) {
        test_fail();
    }

    blocked_sender_done = 1u;

    while (1) {
        os_delay(100u);
    }
}

static void recv_timeout_task(void) {
    queue_test_msg_t out;
    os_status_t status;

    wait_for_phase(PHASE_RECV_TIMEOUT);

    recv_timeout_started = 1u;

    status = os_queue_recv(&queue, &out, TIMEOUT_TICKS);

    if (status != OS_ERR_TIMEOUT) {
        test_fail();
    }

    recv_timeout_done = 1u;

    while (1) {
        os_delay(100u);
    }
}

static void send_timeout_task(void) {
    queue_test_msg_t msg;
    os_status_t status;

    wait_for_phase(PHASE_SEND_TIMEOUT);

    msg = make_raw_msg(0x99999999u);

    send_timeout_started = 1u;

    status = os_queue_send(&queue, &msg, TIMEOUT_TICKS);

    if (status != OS_ERR_TIMEOUT) {
        test_fail();
    }

    send_timeout_done = 1u;

    while (1) {
        os_delay(100u);
    }
}

static void prio_low_recv_task(void) {
    queue_test_msg_t out;
    os_status_t status;

    wait_for_phase(PHASE_PRIO_RECV);

    prio_low_waiting = 1u;

    status = os_queue_recv(&queue, &out, OS_WAIT_FOREVER);

    if (status != OS_OK) {
        test_fail();
    }

    prio_low_value = out.sequence;
    prio_low_done = 1u;

    while (1) {
        os_delay(100u);
    }
}

static void prio_high_recv_task(void) {
    queue_test_msg_t out;
    os_status_t status;

    wait_for_phase(PHASE_PRIO_RECV);

    prio_high_waiting = 1u;

    status = os_queue_recv(&queue, &out, OS_WAIT_FOREVER);

    if (status != OS_OK) {
        test_fail();
    }

    prio_high_value = out.sequence;
    prio_high_done = 1u;

    while (1) {
        os_delay(100u);
    }
}

static uint32_t claim_producer_id(void) {
    uint32_t id;

    id = producer_next_id;
    producer_next_id++;

    return id;
}

static uint32_t claim_consumer_id(void) {
    uint32_t id;

    id = consumer_next_id;
    consumer_next_id++;

    return id;
}

static void producer_task(void) {
    uint32_t producer_id;

    wait_for_phase(PHASE_STRESS);

    producer_id = claim_producer_id();

    if (producer_id >= PRODUCER_COUNT) {
        test_fail();

        while (1) {
            os_delay(100u);
        }
    }

    for (uint32_t i = 0u; i < MESSAGES_PER_PRODUCER; i++) {
        queue_test_msg_t msg;

        msg.producer_id = producer_id;
        msg.sequence = i;
        msg.checksum = make_checksum(msg.producer_id, msg.sequence);

        if (os_queue_send(&queue, &msg, OS_WAIT_FOREVER) != OS_OK) {
            test_fail();
        } else {
            produced_count++;
        }

        os_delay(0u);
    }

    producer_done_count++;

    while (1) {
        os_delay(100u);
    }
}

static void consumer_task(void) {
    uint32_t consumer_id;
    uint32_t target_total;

    wait_for_phase(PHASE_STRESS);

    consumer_id = claim_consumer_id();
    target_total = PRODUCER_COUNT * MESSAGES_PER_PRODUCER;

    if (consumer_id >= CONSUMER_COUNT) {
        test_fail();

        while (1) {
            os_delay(100u);
        }
    }

    while (consumed_count < target_total) {
        queue_test_msg_t msg;

        if (os_queue_recv(&queue, &msg, OS_WAIT_FOREVER) != OS_OK) {
            test_fail();
            continue;
        }

        if (msg.producer_id >= PRODUCER_COUNT) {
            test_fail();
        } else if (msg.sequence >= MESSAGES_PER_PRODUCER) {
            test_fail();
        } else if (msg.checksum != make_checksum(msg.producer_id, msg.sequence)) {
            test_fail();
        } else {
            if (seen[msg.producer_id][msg.sequence] != 0u) {
                test_fail();
            }

            seen[msg.producer_id][msg.sequence] = 1u;
        }

        consumed_count++;

        os_delay(0u);
    }

    consumer_done_count++;

    while (1) {
        os_delay(100u);
    }
}

static void  busy_delay_task(void){
    uint32_t start_tick;
    uint32_t end_tick;
    uint32_t meas_tick;
    while(1){
        start_tick = uwTick;
        os_delay_busy(100u);
        end_tick = uwTick;
        meas_tick = end_tick - start_tick;
        if(meas_tick > 102 || meas_tick < 98){
            test_fail();
        }
    }
}

static void  block_delay_task(void){
    uint32_t start_tick = 0;
    uint32_t end_tick = 0;
    uint32_t meas_tick = 0;
    while(1){
        start_tick = uwTick;
        os_delay(25u);
        end_tick = uwTick;
        meas_tick = end_tick - start_tick;
        
    }
}

static void monitor_task(void) {
    queue_test_msg_t msg;
    queue_test_msg_t out;
    uint32_t expected_total;

    os_delay(100u);

    /*
     * 1. Direct handoff to blocked receiver.
     */
    reset_queue();

    recv_handoff_waiting = 0u;
    recv_handoff_done = 0u;

    test_phase = PHASE_RECV_HANDOFF;

    while (recv_handoff_waiting == 0u) {
        os_delay(1u);
    }

    os_delay(20u);

    msg = make_raw_msg(TEST_MSG_RECV_HANDOFF);

    if (os_queue_send(&queue, &msg, OS_NO_WAIT) != OS_OK) {
        test_fail();
    }

    while (recv_handoff_done == 0u) {
        os_delay(1u);
    }

    if (!os_queue_is_empty(&queue)) {
        test_fail();
    }

    /*
     * 2. Blocked sender copied into freed ring slot.
     */
    reset_queue();

    blocked_sender_started = 0u;
    blocked_sender_done = 0u;

    for (uint32_t i = 0u; i < QUEUE_DEPTH; i++) {
        msg = make_raw_msg(TEST_MSG_BLOCKED_SEND_OLD + i);

        if (os_queue_send(&queue, &msg, OS_NO_WAIT) != OS_OK) {
            test_fail();
        }
    }

    if (!os_queue_is_full(&queue)) {
        test_fail();
    }

    test_phase = PHASE_BLOCKED_SEND;

    while (blocked_sender_started == 0u) {
        os_delay(1u);
    }

    os_delay(20u);

    if (os_queue_recv(&queue, &out, OS_NO_WAIT) != OS_OK) {
        test_fail();
    }

    if (!is_raw_msg(&out, TEST_MSG_BLOCKED_SEND_OLD)) {
        test_fail();
    }

    while (blocked_sender_done == 0u) {
        os_delay(1u);
    }

    for (uint32_t i = 1u; i < QUEUE_DEPTH; i++) {
        if (os_queue_recv(&queue, &out, OS_NO_WAIT) != OS_OK) {
            test_fail();
        }

        if (!is_raw_msg(&out, TEST_MSG_BLOCKED_SEND_OLD + i)) {
            test_fail();
        }
    }

    if (os_queue_recv(&queue, &out, OS_NO_WAIT) != OS_OK) {
        test_fail();
    }

    if (!is_raw_msg(&out, TEST_MSG_BLOCKED_SEND_NEW)) {
        test_fail();
    }

    if (!os_queue_is_empty(&queue)) {
        test_fail();
    }

    /*
     * 3. recv timeout cleanup.
     */
    reset_queue();

    recv_timeout_started = 0u;
    recv_timeout_done = 0u;

    test_phase = PHASE_RECV_TIMEOUT;

    while (recv_timeout_started == 0u) {
        os_delay(1u);
    }

    while (recv_timeout_done == 0u) {
        os_delay(1u);
    }

    if (!os_queue_is_empty(&queue)) {
        test_fail();
    }

    msg = make_raw_msg(0xAAAA0001u);

    if (os_queue_send(&queue, &msg, OS_NO_WAIT) != OS_OK) {
        test_fail();
    }

    if (os_queue_recv(&queue, &out, OS_NO_WAIT) != OS_OK) {
        test_fail();
    }

    if (!is_raw_msg(&out, 0xAAAA0001u)) {
        test_fail();
    }

    /*
     * 4. send timeout cleanup.
     */
    reset_queue();

    send_timeout_started = 0u;
    send_timeout_done = 0u;

    for (uint32_t i = 0u; i < QUEUE_DEPTH; i++) {
        msg = make_raw_msg(0xBBBB0001u + i);

        if (os_queue_send(&queue, &msg, OS_NO_WAIT) != OS_OK) {
            test_fail();
        }
    }

    if (!os_queue_is_full(&queue)) {
        test_fail();
    }

    test_phase = PHASE_SEND_TIMEOUT;

    while (send_timeout_started == 0u) {
        os_delay(1u);
    }

    while (send_timeout_done == 0u) {
        os_delay(1u);
    }

    for (uint32_t i = 0u; i < QUEUE_DEPTH; i++) {
        if (os_queue_recv(&queue, &out, OS_NO_WAIT) != OS_OK) {
            test_fail();
        }

        if (!is_raw_msg(&out, 0xBBBB0001u + i)) {
            test_fail();
        }
    }

    if (!os_queue_is_empty(&queue)) {
        test_fail();
    }

    /*
     * 5. Priority receive wake order diagnostic.
     *
     * Expected if lower numeric priority means higher priority:
     *
     * prio_high_value = TEST_MSG_PRIO_FIRST
     * prio_low_value  = TEST_MSG_PRIO_SECOND
     *
     * If reversed, your prio_waitq ordering is backwards relative to the scheduler.
     */
    reset_queue();

    prio_low_waiting = 0u;
    prio_high_waiting = 0u;
    prio_low_done = 0u;
    prio_high_done = 0u;
    prio_low_value = 0u;
    prio_high_value = 0u;

    test_phase = PHASE_PRIO_RECV;

    while ((prio_low_waiting == 0u) || (prio_high_waiting == 0u)) {
        os_delay(1u);
    }

    os_delay(20u);

    msg = make_raw_msg(TEST_MSG_PRIO_FIRST);

    if (os_queue_send(&queue, &msg, OS_NO_WAIT) != OS_OK) {
        test_fail();
    }

    while ((prio_high_done == 0u) && (prio_low_done == 0u)) {
        os_delay(1u);
    }

    if (prio_low_done != 0u) {
        /*
         * Low-priority receiver got the first message.
         * That means the wait queue did not pop the assumed highest priority task.
         */
        test_fail();
    }

    if (prio_high_done == 0u) {
        test_fail();
    }

    if (prio_high_value != TEST_MSG_PRIO_FIRST) {
        test_fail();
    }

    msg = make_raw_msg(TEST_MSG_PRIO_SECOND);

    if (os_queue_send(&queue, &msg, OS_NO_WAIT) != OS_OK) {
        test_fail();
    }

    while ((prio_low_done == 0u) || (prio_high_done == 0u)) {
        os_delay(1u);
    }

    if (prio_low_value != TEST_MSG_PRIO_SECOND) {
        test_fail();
    }

    if (!os_queue_is_empty(&queue)) {
        test_fail();
    }

    /*
     * 6. Stress test.
     */
    reset_queue();

    produced_count = 0u;
    consumed_count = 0u;
    producer_done_count = 0u;
    consumer_done_count = 0u;
    producer_next_id = 0u;
    consumer_next_id = 0u;

    for (uint32_t p = 0u; p < PRODUCER_COUNT; p++) {
        for (uint32_t i = 0u; i < MESSAGES_PER_PRODUCER; i++) {
            seen[p][i] = 0u;
        }
    }

    expected_total = PRODUCER_COUNT * MESSAGES_PER_PRODUCER;

    test_phase = PHASE_STRESS;

    while (consumer_done_count < CONSUMER_COUNT) {
        os_delay(10u);
    }

    if (producer_done_count != PRODUCER_COUNT) {
        test_fail();
    }

    if (produced_count != expected_total) {
        test_fail();
    }

    if (consumed_count != expected_total) {
        test_fail();
    }

    for (uint32_t p = 0u; p < PRODUCER_COUNT; p++) {
        for (uint32_t i = 0u; i < MESSAGES_PER_PRODUCER; i++) {
            if (seen[p][i] == 0u) {
                test_fail();
            }
        }
    }

    if (!os_queue_is_empty(&queue)) {
        test_fail();
    }

    if (test_error_count == 0u) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    }

    while (1) {
        os_delay(100u);
    }
}


void app_tasks_init(void) {
    
    basic_queue_tests();
    
    /*
    * Create monitor last but give it highest priority.
    * Assumption: lower numeric value means higher priority.
    */
    
    if (os_task_create(recv_handoff_task, 4u) != OS_OK) {
        test_fail();
    }
    
    if (os_task_create(blocked_sender_task, 4u) != OS_OK) {
        test_fail();
    }
    
    if (os_task_create(recv_timeout_task, 4u) != OS_OK) {
        test_fail();
    }

    if (os_task_create(send_timeout_task, 4u) != OS_OK) {
        test_fail();
    }
    
    /*
    * Low receiver is created before high receiver on purpose.
    * If the wait queue is FIFO instead of priority ordered, the low receiver
    * will incorrectly get TEST_MSG_PRIO_FIRST.
    */
    if (os_task_create(prio_low_recv_task, 3u) != OS_OK) {
        test_fail();
    }
    
    if (os_task_create(prio_high_recv_task, 5u) != OS_OK) {
        test_fail();
    }
    
    for (uint32_t p = 0u; p < PRODUCER_COUNT; p++) {
        if (os_task_create(producer_task, 3u) != OS_OK) {
            test_fail();
        }
    }
    
    for (uint32_t c = 0u; c < CONSUMER_COUNT; c++) {
        if (os_task_create(consumer_task, 3u) != OS_OK) {
            test_fail();
        }
    }

    if (os_task_create(monitor_task, 7u) != OS_OK) {
        test_fail();
    }
    
    if(os_task_create(busy_delay_task, 3u) != OS_OK){
        test_fail();
    }
    if(os_task_create(block_delay_task, 5u) != OS_OK){
        test_fail();
    }
}