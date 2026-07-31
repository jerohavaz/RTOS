/**
 * @file port.h
 * @brief Cortex-M port abstraction used by the RTOS kernel.
 * @author Jerome
 *
 * @details
 * Declares the CPU-port types and services required by the RTOS kernel.
 * Kernel modules use this interface for task-stack initialization, scheduling
 * requests, critical sections, execution-context detection, idle waiting, and
 * fatal-error handling.
 */

#ifndef PORT_H_
#define PORT_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Native stack word type for this CPU port.
 *
 * A task stack is represented as an array of these entries.
 */
typedef uint32_t port_stack_t;

/**
 * @brief Task entry function type.
 *
 * Tasks take no parameters and return no value. Returning transfers control
 * to the configured @ref port_task_exit_t handler.
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
 * Applies the scheduler interrupt priorities defined by the OS configuration.
 *
 * @pre The configured priorities must be valid for the selected CPU port.
 */
void port_init_scheduler_interrupts(void);

/**
 * @brief Start the first scheduled task.
 *
 * Transfers control to the architecture-specific first-task startup path.
 * This function does not return during normal operation.
 *
 * @pre The scheduler must have selected a task with a valid initialized stack.
 */
void port_start_first_task(void);

/**
 * @brief Request a context switch.
 *
 * Requests deferred scheduler processing. The context switch may occur after
 * this function returns.
 */
void port_request_context_switch(void);

/**
 * @brief Build the initial stack frame for a new task.
 *
 * Creates the port-specific initial context required to start a task.
 *
 * @param stack_base Pointer to the first element of the task stack buffer.
 * @param stack_words Number of @ref port_stack_t entries in the stack buffer.
 * @param entry Non-null task entry function.
 * @param exit_handler Non-null function entered if the task function returns.
 *
 * @return Pointer to the initialized software-saved context.
 * @retval NULL An argument is invalid or the buffer is too small.
 *
 * @note The returned stack pointer satisfies the alignment requirements of
 *       the selected port.
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
 * interrupt state and does not return it to the caller.
 *
 * @note Use @ref port_enter_critical and @ref port_exit_critical for normal
 *       nestable kernel critical sections.
 */
void port_disable_interrupts(void);

/**
 * @brief Enter a kernel critical section.
 *
 * Masks interrupts that are permitted to call the kernel while preserving the
 * previous port-specific interrupt-mask state.
 *
 * @return Previous port-specific interrupt-mask state.
 *
 * @note The returned value must be passed unchanged to the matching
 *       @ref port_exit_critical call.
 */
uint32_t port_enter_critical(void);

/**
 * @brief Exit a kernel critical section.
 *
 * Restores the interrupt-mask state returned by port_enter_critical().
 *
 * @param previous_basepri Interrupt-mask value returned by the matching
 *                         @ref port_enter_critical call.
 */
void port_exit_critical(uint32_t previous_basepri);

/**
 * @brief Return the currently active exception number.
 *
 * @return Port-defined active exception identifier.
 * @retval 0 The processor is not running in exception context.
 */
uint32_t port_get_active_exception_id(void);

/**
 * @brief Check whether the CPU is currently running in exception context.
 *
 * @retval true The CPU is running inside an exception handler.
 * @retval false The CPU is running in thread mode.
 */
bool port_in_exception(void);

/* -------------------------------------------------------------------------- */
/* Idle / debug                                                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Wait for interrupt.
 *
 * Suspends execution until an interrupt or another port-defined wake event
 * occurs. Intended for use by the idle task.
 */
void port_wait_for_interrupt(void);

/**
 * @brief Execute one no-operation instruction.
 */
void port_no_operation(void);

/**
 * @brief Trigger a debugger breakpoint.
 */
void port_breakpoint(void);

/**
 * @brief Halt the system.
 *
 * Disables interrupts and enters a non-returning debug halt loop.
 */
void port_halt(void);

#endif /* PORT_H_ */