/**
 * @file os_isr.h
 * @brief RTOS interrupt lifecycle and system-tick hooks.
 * @author Jerome
 *
 * Provides the hooks used by the platform interrupt handlers to report ISR
 * entry and exit, advance kernel time, process expired waits, and request
 * scheduling before returning to Thread mode.
 */
#ifndef OS_ISR_H_
#define OS_ISR_H_

#include <stdint.h>

/**
 * @brief Report entry into an RTOS-aware interrupt handler.
 *
 * Records ISR entry for the enabled trace backends.
 *
 * @note Pair each call with os_isr_exit() on every handler exit path.
 */
void os_isr_enter(void);

/**
 * @brief Process one system-tick interrupt.
 *
 * Once the scheduler has started, increments the kernel tick, records the tick
 * trace event, and processes all timeouts that have expired at the new tick.
 * The function has no effect before scheduler startup.
 *
 * @note This function does not request a context switch directly. Call
 *       os_isr_exit() afterward to apply scheduling and trace the ISR exit.
 */
void os_systick_tick(void);

/**
 * @brief Complete an RTOS-aware interrupt handler.
 *
 * Before scheduler startup, records a normal ISR exit. After startup, applies
 * yield scheduling: if a switch is required, PendSV is pended and the exit is
 * traced as a scheduler handoff; otherwise, a normal ISR exit is recorded.
 *
 * @note When PendSV is pending, Cortex-M tail-chains into the context-switch
 *       handler before Thread mode resumes. That interval is therefore
 *       accounted as scheduler time by the trace backend.
 */
void os_isr_exit(void);

#endif /* OS_ISR_H_ */