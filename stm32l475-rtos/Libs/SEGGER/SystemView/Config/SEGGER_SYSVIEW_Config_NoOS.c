/**
 * @file SEGGER_SYSVIEW_Config_NoOS.c
 * @brief SEGGER SystemView target configuration for the custom RTOS.
 * @author Jerome
 *
 * @details
 * Configures the Cortex-M DWT cycle counter, target frequencies, RAM base, and
 * SystemView system description. The SystemView OS API itself is implemented
 * by the RTOS trace subsystem. In particular, its task-list callback replays
 * trace-owned task metadata and therefore does not query kernel internals.
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
 * @brief SystemView OS integration supplied by the trace subsystem.
 *
 * The object is defined in @c trace.c when the SystemView backend is enabled.
 * Keeping the definition there lets the task-list callback access the
 * trace-owned task registry without introducing a dependency on the kernel.
 */
extern const SEGGER_SYSVIEW_OS_API g_trace_sysview_os_api;

/**
 * @brief Send the static SystemView target/system description.
 *
 * The @c O= field selects @c .sysview/SYSVIEW_CustomRTOS.txt on the host.
 * Interrupt 15 is named explicitly so SysTick is recognizable in the trace.
 */
static void _cbSendSystemDesc(void) {
    SEGGER_SYSVIEW_SendSysDesc("N=" SYSVIEW_APP_NAME ",O=" SYSVIEW_OS_NAME ",D=" SYSVIEW_DEVICE_NAME
                               ",C=Cortex-M4");

    SEGGER_SYSVIEW_SendSysDesc("I#15=SysTick");
}

/**
 * @brief Configure SEGGER SystemView for the STM32L475 target.
 *
 * Enables the DWT cycle counter when implemented, initializes SystemView with
 * the RTOS trace OS API, and configures the SRAM base used for compressed
 * pointer-like object identifiers.
 */
void SEGGER_SYSVIEW_Conf(void) {
    DEMCR |= TRACEENA_BIT;

    if ((DWT_CTRL & NOCYCCNT_BIT) == 0u) {
        DWT_CYCCNT = 0u;
        DWT_CTRL |= CYCCNTENA_BIT;
    }

    SEGGER_SYSVIEW_Init(SYSVIEW_TIMESTAMP_FREQ,
                        SYSVIEW_CPU_FREQ,
                        &g_trace_sysview_os_api,
                        _cbSendSystemDesc);

    SEGGER_SYSVIEW_SetRAMBase(SYSVIEW_RAM_BASE);
}
