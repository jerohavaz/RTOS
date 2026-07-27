#include "app_tasks.h"
#include "os_task.h"
#include "os_delay.h"
#include "os_types.h"
#include "stm32l4xx_hal.h"

#define BUSY_DELAY_TICKS  100u
#define BLOCK_DELAY_TICKS 25u
#define DELAY_TOLERANCE   2u

volatile uint32_t test_error_count = 0u;

static void test_fail(void) {
    test_error_count++;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
}

static uint8_t delay_is_in_range(uint32_t measured, uint32_t expected) {
    return (measured >= (expected - DELAY_TOLERANCE)) &&
           (measured <= (expected + DELAY_TOLERANCE));
}

static void busy_delay_task(void) {
    while (1) {
        uint32_t start_tick = uwTick;

        os_delay_busy(BUSY_DELAY_TICKS);

        if (!delay_is_in_range(uwTick - start_tick, BUSY_DELAY_TICKS)) {
            test_fail();
        }
    }
}

static void block_delay_task(void) {
    while (1) {
        uint32_t start_tick = uwTick;

        os_delay(BLOCK_DELAY_TICKS);

        if (!delay_is_in_range(uwTick - start_tick, BLOCK_DELAY_TICKS)) {
            test_fail();
        }
    }
}

void app_tasks_init(void) {
    if (os_task_create(busy_delay_task, 3u) != OS_OK) {
        test_fail();
    }

    if (os_task_create(block_delay_task, 5u) != OS_OK) {
        test_fail();
    }
}