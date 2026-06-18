#ifndef PORT_H_
#define PORT_H_

#include <stdint.h>

/**
 * @file port.h
 * @brief Cortex-M port abstraction used by the RTOS kernel.
 *
 * This header exposes CPU-specific primitives behind a small port interface.
 * Kernel code may use these functions, but it must not directly access Cortex-M
 * registers, exception names, NVIC functions, or assembly instructions.
 */

/**
 * @brief Native stack word type for this CPU port.
 *
 * Cortex-M stacks are word-addressed and use 32-bit registers, so one stack
 * entry is 32 bits.
 */
typedef uint32_t port_stack_t;

/**
 * @brief Task entry function type.
 */
typedef void (*port_task_entry_t)(void);

/**
 * @brief Task exit handler function type.
 *
 * Used as the initial LR value for a task. If a task function returns, control
 * is transferred to this handler.
 */
typedef void (*port_task_exit_t)(void);

/* -------------------------------------------------------------------------- */
/* Scheduler / context switching                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize architecture-specific scheduler interrupt settings.
 *
 * Configures the exception priorities used by the RTOS, such as PendSV,
 * SysTick, and SVC. Priority values are taken from the OS configuration.
 */
void port_init_scheduler_interrupts(void);

/**
 * @brief Start the first scheduled task.
 *
 * Transfers control to the architecture-specific first-task startup path.
 * This function does not return during normal operation.
 */
void port_start_first_task(void);

/**
 * @brief Request a context switch.
 *
 * Pends the PendSV exception. The actual context switch occurs when PendSV
 * executes.
 */
void port_request_context_switch(void);

/**
 * @brief Build the initial stack frame for a new task.
 *
 * Creates the architecture-specific initial stack layout expected by the
 * context restore code. On Cortex-M this includes the hardware exception frame
 * and the software-saved callee registers.
 *
 * @param stack_base Pointer to the first element of the task stack buffer.
 * @param stack_words Number of port_stack_t entries in the stack buffer.
 * @param entry Task entry function.
 * @param exit_handler Function entered if the task function returns.
 *
 * @return Initial task stack pointer.
 * @retval 0 Invalid argument.
 */
port_stack_t *port_init_task_stack(port_stack_t *stack_base,
                                   uint32_t stack_words,
                                   port_task_entry_t entry,
                                   port_task_exit_t exit_handler);

/* -------------------------------------------------------------------------- */
/* Interrupt control / critical sections                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Disable all normal maskable interrupts.
 *
 * This is a one-way startup/panic helper. It does not preserve the previous
 * interrupt state.
 *
 * Use port_enter_critical() and port_exit_critical() for normal kernel
 * critical sections.
 */
void port_disable_interrupts(void);

/**
 * @brief Enter a kernel critical section.
 *
 * Raises BASEPRI so interrupts at or below the configured kernel interrupt
 * priority cannot run. Higher-urgency interrupts remain enabled.
 *
 * The returned value must be passed to port_exit_critical() to restore the
 * previous BASEPRI state.
 *
 * @return Previous BASEPRI value.
 */
uint32_t port_enter_critical(void);

/**
 * @brief Exit a kernel critical section.
 *
 * Restores the BASEPRI value returned by port_enter_critical().
 *
 * @param previous_basepri Previous BASEPRI value.
 */
void port_exit_critical(uint32_t previous_basepri);

/**
 * @brief Return the currently active exception number.
 *
 * On Cortex-M this reads IPSR. A return value of 0 means thread mode.
 *
 * @return Active exception number, or 0 when not inside an exception.
 */
uint32_t port_get_active_exception_id(void);

/* -------------------------------------------------------------------------- */
/* Idle / debug                                                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Wait for interrupt.
 *
 * Executes the Cortex-M WFI instruction. Intended for use by the idle task.
 */
void port_wait_for_interrupt(void);

/**
 * @brief Execute one no-operation instruction.
 */
void port_no_operation(void);

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

#endif /* PORT_H_ */