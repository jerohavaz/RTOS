#ifndef PORT_H_
#define PORT_H_

#include <stdint.h>

/**
 * @file port.h
 * @brief Cortex-M port abstraction for low-level RTOS operations.
 *
 * This module contains CPU-specific primitives used by the kernel, such as
 * interrupt masking, PendSV triggering, idle waiting, and debug halt support.
 */

/**
 * @brief Start the first scheduled task.
 *
 * This function transfers control to the architecture-specific first-task
 * startup code. It does not return during normal operation.
 */
void port_start_first_task(void);

/**
 * @brief Request a context switch.
 *
 * Pends the PendSV exception. The actual context switch is performed when
 * PendSV executes.
 */
void port_request_context_switch(void);

/**
 * @brief Disable all normal maskable interrupts.
 *
 * This is a one-way startup/panic helper. It does not return or preserve the
 * previous interrupt state.
 *
 * Use port_enter_critical() and port_exit_critical() for normal critical
 * sections.
 */
void port_disable_interrupts(void);

/**
 * @brief Enter a critical section.
 *
 * Saves the current PRIMASK value and disables normal maskable interrupts.
 * The returned value must be passed to port_exit_critical() to restore the
 * previous interrupt state.
 *
 * @return Previous PRIMASK value.
 */
uint32_t port_enter_critical(void);

/**
 * @brief Exit a critical section.
 *
 * Restores the PRIMASK value returned by port_enter_critical().
 *
 * @param primask Previous PRIMASK value returned by port_enter_critical().
 */
void port_exit_critical(uint32_t primask);

/**
 * @brief Trigger a debugger breakpoint.
 *
 * If a debugger is attached, execution stops at the breakpoint.
 */
void port_breakpoint(void);

/**
 * @brief Halt the system.
 *
 * Disables interrupts and repeatedly triggers a breakpoint.
 */
void port_halt(void);

/**
 * @brief Wait for interrupt.
 *
 * Executes the Cortex-M WFI instruction. Intended for use by the idle task.
 */
void port_wait_for_interrupt(void);

#endif /* PORT_H */