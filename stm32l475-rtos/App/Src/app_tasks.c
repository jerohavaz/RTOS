#include "app_tasks.h"
#include "os_sem.h"
#include "os_task.h"
#include "stm32l4xx_hal.h"

enum { WAKE_NONE, WAKE_HIGH, WAKE_LOW };

static os_sem_t sem_start_high;
static os_sem_t sem_start_low;
static os_sem_t sem_gate;
static os_sem_t sem_sync;

volatile uint32_t test_error_count = 0u;
volatile uint32_t stress_round_count = 0u;
volatile uint32_t high_wake_count = 0u;
volatile uint32_t low_wake_count = 0u;
volatile uint8_t expected_wake = WAKE_NONE;

static void test_fail(void) {
    test_error_count++;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
}

static void expect_status(os_status_t actual, os_status_t expected) {
    if (actual != expected) {
        test_fail();
    }
}

static void waiter_loop(os_sem_t *start, uint8_t waiter_id, volatile uint32_t *wake_count) {
    while (1) {
        if (os_sem_acquire(start, OS_WAIT_FOREVER) != OS_OK) {
            test_fail();
            continue;
        }

        /* Ready: this task will now block on the shared gate. */
        expect_status(os_sem_release(&sem_sync), OS_OK);

        if (os_sem_acquire(&sem_gate, OS_WAIT_FOREVER) != OS_OK) {
            test_fail();
            continue;
        }

        if (expected_wake != waiter_id) {
            test_fail();
        }

        expected_wake = WAKE_NONE;
        (*wake_count)++;

        /* Handoff completed. */
        expect_status(os_sem_release(&sem_sync), OS_OK);
    }
}

static void high_waiter_task(void) {
    waiter_loop(&sem_start_high, WAKE_HIGH, &high_wake_count);
}

static void low_waiter_task(void) {
    waiter_loop(&sem_start_low, WAKE_LOW, &low_wake_count);
}

static void controller_task(void) {
    /*
     * One-time counting semaphore and timeout checks.
     * sem_sync has count 0 and maximum count 2.
     */
    expect_status(os_sem_acquire(&sem_sync, OS_NO_WAIT), OS_ERR_WOULD_BLOCK);

    expect_status(os_sem_release(&sem_sync), OS_OK);
    expect_status(os_sem_release(&sem_sync), OS_OK);
    expect_status(os_sem_release(&sem_sync), OS_ERR_FULL);

    expect_status(os_sem_acquire(&sem_sync, OS_NO_WAIT), OS_OK);
    expect_status(os_sem_acquire(&sem_sync, OS_NO_WAIT), OS_OK);
    expect_status(os_sem_acquire(&sem_sync, OS_NO_WAIT), OS_ERR_WOULD_BLOCK);

    expect_status(os_sem_acquire(&sem_sync, 3u), OS_ERR_TIMEOUT);
    expect_status(os_sem_acquire(&sem_sync, OS_NO_WAIT), OS_ERR_WOULD_BLOCK);

    while (1) {
        uint32_t next_round = stress_round_count + 1u;

        /*
         * Queue low first, then high. Despite insertion order, the
         * higher-priority task must receive the first released token.
         */
        expected_wake = WAKE_HIGH;

        expect_status(os_sem_release(&sem_start_low), OS_OK);
        expect_status(os_sem_acquire(&sem_sync, OS_WAIT_FOREVER), OS_OK);

        expect_status(os_sem_release(&sem_start_high), OS_OK);
        expect_status(os_sem_acquire(&sem_sync, OS_WAIT_FOREVER), OS_OK);

        expect_status(os_sem_release(&sem_gate), OS_OK);
        expect_status(os_sem_acquire(&sem_sync, OS_WAIT_FOREVER), OS_OK);

        if ((expected_wake != WAKE_NONE) || (high_wake_count != next_round) ||
            (low_wake_count != stress_round_count)) {
            test_fail();
        }

        expected_wake = WAKE_LOW;

        expect_status(os_sem_release(&sem_gate), OS_OK);
        expect_status(os_sem_acquire(&sem_sync, OS_WAIT_FOREVER), OS_OK);

        if ((expected_wake != WAKE_NONE) || (high_wake_count != next_round) ||
            (low_wake_count != next_round)) {
            test_fail();
        }

        stress_round_count = next_round;
    }
}

void app_tasks_init(void) {
    /* Public argument-validation paths. */
    expect_status(os_sem_init(0, 0u, 1u), OS_ERR_NULL);
    expect_status(os_sem_init(&sem_sync, 0u, 0u), OS_ERR_INVALID_ARG);
    expect_status(os_sem_init(&sem_sync, 2u, 1u), OS_ERR_INVALID_ARG);

    expect_status(os_sem_init(&sem_start_high, 0u, 1u), OS_OK);
    expect_status(os_sem_init(&sem_start_low, 0u, 1u), OS_OK);
    expect_status(os_sem_init(&sem_gate, 0u, 1u), OS_OK);
    expect_status(os_sem_init(&sem_sync, 0u, 2u), OS_OK);

    expect_status(os_sem_acquire(0, OS_NO_WAIT), OS_ERR_NULL);
    expect_status(os_sem_release(0), OS_ERR_NULL);

    expect_status(os_task_create(high_waiter_task, 3u), OS_OK);
    expect_status(os_task_create(low_waiter_task, 2u), OS_OK);
    expect_status(os_task_create(controller_task, 1u), OS_OK);
}