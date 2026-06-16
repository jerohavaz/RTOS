#include <stdint.h>

#include "app_tasks.h"
#include "os_delay.h"
#include "stm32l4xx_hal.h"

void app_task1(void) {
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
        os_delay(500);
    }
}

void app_task2(void) {
    while (1) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        os_delay(1000);
    }
}
