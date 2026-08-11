/**
 * @file SEGGER_SYSVIEW_Conf.c
 * @brief SEGGER SystemView configuration for the custom RTOS.
 * @author Jerome
 */
#include "SEGGER_SYSVIEW.h"
#include "SEGGER_SYSVIEW_Conf.h"
#include "stm32l4xx.h"

#define SYSVIEW_APP_NAME    "RTOS"
#define SYSVIEW_OS_NAME     "CustomRTOS"
#define SYSVIEW_DEVICE_NAME "STM32L475VGT6"

#define SYSVIEW_RAM_BASE 0x20000000u

#define SYSVIEW_CPU_FREQ       (SystemCoreClock)
#define SYSVIEW_TIMESTAMP_FREQ (SystemCoreClock)

#define DEMCR      (*(volatile unsigned long *)0xE000EDFCuL)
#define DWT_CTRL   (*(volatile unsigned long *)0xE0001000uL)
#define DWT_CYCCNT (*(volatile unsigned long *)0xE0001004uL)

#define TRACEENA_BIT  (1uL << 24)
#define CYCCNTENA_BIT (1uL << 0)
#define NOCYCCNT_BIT  (1uL << 25)

/**
 * @brief Send the static SystemView system description.
 *
 * The O= field selects the host-side OS description file
 * SYSVIEW_CustomRTOS.txt. Interrupt 15 is named SysTick.
 */
static void _cbSendSystemDesc(void) {
    SEGGER_SYSVIEW_SendSysDesc("N=" SYSVIEW_APP_NAME ",O=" SYSVIEW_OS_NAME ",D=" SYSVIEW_DEVICE_NAME
                               ",C=Cortex-M4");

    SEGGER_SYSVIEW_SendSysDesc("I#15=SysTick");
}

void SEGGER_SYSVIEW_Conf(void) {
    DEMCR |= TRACEENA_BIT;

    if ((DWT_CTRL & NOCYCCNT_BIT) == 0u) {
        DWT_CYCCNT = 0u;
        DWT_CTRL |= CYCCNTENA_BIT;
    }

    SEGGER_SYSVIEW_Init(SYSVIEW_TIMESTAMP_FREQ, SYSVIEW_CPU_FREQ, 0, _cbSendSystemDesc);

    SEGGER_SYSVIEW_SetRAMBase(SYSVIEW_RAM_BASE);
}
