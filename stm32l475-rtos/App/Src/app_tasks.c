#include "app_tasks.h"
#include "os_mutex.h"
#include "os_sem.h"
#include "os_task.h"
#include "stm32l4xx_hal.h"

enum { WAKE_NONE, WAKE_HIGH, WAKE_LOW };

enum { LOW_MODE_TIMEOUT, LOW_MODE_WAIT };

static os_mutex_t test_mutex;
static os_sem_t sem_start_high;
static os_sem_t sem_start_low;
static os_sem_t sem_sync;

volatile uint32_t test_error_count = 0u;
volatile uint32_t stress_round_count = 0u;
volatile uint32_t high_handoff_count = 0u;
volatile uint32_t low_handoff_count = 0u;
volatile uint8_t expected_wake = WAKE_NONE;
volatile uint8_t low_mode = LOW_MODE_TIMEOUT;

static void test_fail(void) {
    test_error_count++;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
}

static void expect_status(os_status_t actual, os_status_t expected) {
    if (actual != expected) {
        test_fail();
    }
}

static void high_waiter_task(void) {
    while (1) {
        expect_status(os_sem_acquire(&sem_start_high, OS_WAIT_FOREVER), OS_OK);

        /* Announce that the high-priority task is about to block on the mutex. */
        expect_status(os_sem_release(&sem_sync), OS_OK);
        expect_status(os_mutex_lock(&test_mutex, OS_WAIT_FOREVER), OS_OK);

        if (expected_wake != WAKE_HIGH) {
            test_fail();
        }

        expected_wake = WAKE_LOW;
        high_handoff_count++;

        /* The mutex is non-recursive, including after direct handoff. */
        expect_status(os_mutex_lock(&test_mutex, OS_NO_WAIT), OS_ERR_INVALID_STATE);
        expect_status(os_mutex_unlock(&test_mutex), OS_OK);

        expect_status(os_sem_release(&sem_sync), OS_OK);
    }
}

static void low_waiter_task(void) {
    while (1) {
        expect_status(os_sem_acquire(&sem_start_low, OS_WAIT_FOREVER), OS_OK);

        if (low_mode == LOW_MODE_TIMEOUT) {
            /* The controller owns the mutex throughout this probe. */
            expect_status(os_mutex_unlock(&test_mutex), OS_ERR_NOT_OWNER);
            expect_status(os_mutex_lock(&test_mutex, OS_NO_WAIT), OS_ERR_WOULD_BLOCK);
            expect_status(os_mutex_lock(&test_mutex, 3u), OS_ERR_TIMEOUT);
            expect_status(os_sem_release(&sem_sync), OS_OK);
            continue;
        }

        /* Queue this lower-priority waiter before the high-priority waiter. */
        expect_status(os_sem_release(&sem_sync), OS_OK);
        expect_status(os_mutex_lock(&test_mutex, OS_WAIT_FOREVER), OS_OK);

        if (expected_wake != WAKE_LOW) {
            test_fail();
        }

        expected_wake = WAKE_NONE;
        low_handoff_count++;

        expect_status(os_mutex_unlock(&test_mutex), OS_OK);
        expect_status(os_sem_release(&sem_sync), OS_OK);
    }
}

static void controller_task(void) {
    expect_status(os_mutex_lock(&test_mutex, OS_NO_WAIT), OS_OK);

    /* An owner may not acquire this non-recursive mutex twice. */
    expect_status(os_mutex_lock(&test_mutex, OS_NO_WAIT), OS_ERR_INVALID_STATE);

    /*
     * Exercise non-owner unlock, non-blocking contention and finite timeout.
     * Waiting on sem_sync keeps the controller from releasing the mutex early.
     */
    low_mode = LOW_MODE_TIMEOUT;
    expect_status(os_sem_release(&sem_start_low), OS_OK);
    expect_status(os_sem_acquire(&sem_sync, OS_WAIT_FOREVER), OS_OK);

    while (1) {
        uint32_t next_round = stress_round_count + 1u;

        low_mode = LOW_MODE_WAIT;
        expected_wake = WAKE_HIGH;

        /* Insert low first and wait until it has reached the mutex path. */
        expect_status(os_sem_release(&sem_start_low), OS_OK);
        expect_status(os_sem_acquire(&sem_sync, OS_WAIT_FOREVER), OS_OK);

        /* Insert high second; priority must override insertion order. */
        expect_status(os_sem_release(&sem_start_high), OS_OK);
        expect_status(os_sem_acquire(&sem_sync, OS_WAIT_FOREVER), OS_OK);

        /* Direct handoff must select high, then high must hand off to low. */
        expect_status(os_mutex_unlock(&test_mutex), OS_OK);
        expect_status(os_sem_acquire(&sem_sync, OS_WAIT_FOREVER), OS_OK);
        expect_status(os_sem_acquire(&sem_sync, OS_WAIT_FOREVER), OS_OK);

        if ((expected_wake != WAKE_NONE) || (high_handoff_count != next_round) ||
            (low_handoff_count != next_round)) {
            test_fail();
        }

        stress_round_count = next_round;

        /* Low released the mutex without a waiter, so it must be free again. */
        expect_status(os_mutex_lock(&test_mutex, OS_NO_WAIT), OS_OK);
        expect_status(os_mutex_lock(&test_mutex, OS_NO_WAIT), OS_ERR_INVALID_STATE);
    }
}

void app_tasks_init(void) {
    expect_status(os_mutex_init(0), OS_ERR_NULL);
    expect_status(os_mutex_init(&test_mutex), OS_OK);

    expect_status(os_sem_init(&sem_start_high, 0u, 1u), OS_OK);
    expect_status(os_sem_init(&sem_start_low, 0u, 1u), OS_OK);
    expect_status(os_sem_init(&sem_sync, 0u, 2u), OS_OK);

    expect_status(os_mutex_lock(0, OS_NO_WAIT), OS_ERR_NULL);
    expect_status(os_mutex_unlock(0), OS_ERR_NULL);

    expect_status(os_task_create(high_waiter_task, 3u), OS_OK);
    expect_status(os_task_create(low_waiter_task, 2u), OS_OK);
    expect_status(os_task_create(controller_task, 1u), OS_OK);
}