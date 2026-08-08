#ifndef SHELL_H
#define SHELL_H

#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_uart.h"

void shell_init(UART_HandleTypeDef *huart);
void shell_update(void);
uint8_t is_stream_enabled();

#endif /* SHELL_H */