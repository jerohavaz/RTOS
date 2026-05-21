#include <stdint.h>

#include "app_tasks.h"
#include "cmsis_gcc.h"
#include "stm32l4xx_hal.h"

void busy_loop_delay(volatile uint32_t count) {
    while (count-- > 0u) {
        __NOP();
    }
}

void app_task1(void) {
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
        busy_loop_delay(100000u);
    }
}

void app_task2(void) {
    while (1) {
        busy_loop_delay(100000u);
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
        busy_loop_delay(10000u);
    }
}
