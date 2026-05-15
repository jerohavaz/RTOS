#include <stdint.h>

#include "app_tasks.h"
#include "cmsis_gcc.h"
#include "stm32l4xx_hal.h"

void BusyLoop_Delay(volatile uint32_t count) {
    while (count-- > 0u) {
        __NOP();
    }
}

void App_Task1(void) {
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
        BusyLoop_Delay(100000u);
    }
}

void App_Task2(void) {
    while (1) {
        BusyLoop_Delay(100000u);
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
        BusyLoop_Delay(10000u);
    }
}

void App_IdleTask() {
    while (1) {
        __WFI();
    }
}