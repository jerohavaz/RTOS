#include "app_tasks.h"
#include "os_sem.h"
#include "os_task.h"
#include "os_types.h"
#include "stm32l4xx_hal.h"

static os_sem_t sem_ping;
static os_sem_t sem_pong;

volatile uint32_t test_error_count = 0;
volatile uint32_t ping_count = 0;
volatile uint32_t pong_count = 0;
volatile uint8_t expected_turn = 0u;

static void test_fail(void) {
    test_error_count++;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
}

static void ping_task(void) {
    while (1) {
        if (os_sem_acquire(&sem_ping, OS_WAIT_FOREVER) != OS_OK) {
            test_fail();
        }

        if (expected_turn != 0u) {
            test_fail();
        }

        expected_turn = 1u;
        ping_count++;

        if (os_sem_release(&sem_pong) != OS_OK) {
            test_fail();
        }
    }
}

static void pong_task(void) {
    while (1) {
        if (os_sem_acquire(&sem_pong, OS_WAIT_FOREVER) != OS_OK) {
            test_fail();
        }

        if (expected_turn != 1u) {
            test_fail();
        }

        expected_turn = 0u;
        pong_count++;

        if (os_sem_release(&sem_ping) != OS_OK) {
            test_fail();
        }
    }
}

void sem_test_init(void) {
    os_sem_init(&sem_ping, 1u, 1u);
    os_sem_init(&sem_pong, 0u, 1u);

    if (os_task_create(ping_task, 2u) != OS_OK) {
        test_fail();
    }

    if (os_task_create(pong_task, 2u) != OS_OK) {
        test_fail();
    }
}