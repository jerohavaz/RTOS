#include "app_tasks.h"
#include "os_mutex.h"
#include "os_task.h"
#include "os_delay.h"
#include "os_types.h"
#include "stm32l4xx_hal.h"

#define WORKER_COUNT        4u
#define ITERATIONS_PER_TASK 10000u

static os_mutex_t mutex;

volatile uint32_t test_error_count = 0;
volatile uint32_t shared_counter = 0;
volatile uint32_t done_count = 0;
volatile uint8_t inside_cs = 0u;

static void test_fail(void) {
    test_error_count++;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
}

static void burn_cycles(void) {
    volatile uint32_t i;

    for (i = 0u; i < 200u; i++) {
        __asm volatile ("nop");
    }
}

static void worker_task(void) {
    uint32_t i;

    for (i = 0u; i < ITERATIONS_PER_TASK; i++) {
        uint32_t before;

        if (os_mutex_lock(&mutex, OS_WAIT_FOREVER) != OS_OK) {
            test_fail();
            continue;
        }

        /*
         * Detect two tasks entering the critical section at once.
         */
        if (inside_cs != 0u) {
            test_fail();
        }

        inside_cs = 1u;

        before = shared_counter;

        /*
         * Widen the race window. If the mutex is broken,
         * lost updates become much more likely.
         */
        burn_cycles();

        shared_counter = before + 1u;

        inside_cs = 0u;

        if (os_mutex_unlock(&mutex) != OS_OK) {
            test_fail();
        }

        /*
         * Encourage same-priority interleaving.
         * If os_delay(0) means yield in your OS, this is useful.
         */
        os_delay(0u);
    }

    if (os_mutex_lock(&mutex, OS_WAIT_FOREVER) != OS_OK) {
        test_fail();
    }

    done_count++;

    if (os_mutex_unlock(&mutex) != OS_OK) {
        test_fail();
    }

    while (1) {
        os_delay(100u);
    }
}

static void monitor_task(void) {
    while (done_count < WORKER_COUNT) {
        os_delay(10u);
    }

    if (shared_counter != (WORKER_COUNT * ITERATIONS_PER_TASK)) {
        test_fail();
    }

    /*
     * Optional success LED if you have one.
     */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);

    while (1) {
        os_delay(100u);
    }
}

void mutex_test_init(void) {
    if (os_mutex_init(&mutex) != OS_OK) {
        test_fail();
    }

    for (uint32_t i = 0u; i < WORKER_COUNT; i++) {
        if (os_task_create(worker_task, 2u) != OS_OK) {
            test_fail();
        }
    }

    if (os_task_create(monitor_task, 1u) != OS_OK) {
        test_fail();
    }
}