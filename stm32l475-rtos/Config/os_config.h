/**
 * @file os_config.h
 * @brief Compile-time configuration for the RTOS kernel and Cortex-M port.
 * @author Jerome
 *
 * @details
 * This header defines the kernel's static resource limits, task-priority
 * range, tracing options, and Cortex-M exception priorities. All values are
 * compile-time constants and therefore affect the kernel's memory layout or
 * generated code.
 *
 * @note Larger numeric task-priority values represent higher scheduling
 *       priorities.
 * @note Cortex-M exception priorities use the logical, unshifted priority
 *       values expected by CMSIS @c NVIC_SetPriority().
 */
#ifndef OS_CONFIG_H_
#define OS_CONFIG_H_

#include <stdbool.h>

/**
 * @defgroup OS_CONFIG_TASKS Task configuration
 * @brief Static task limits and per-task stack allocation.
 * @{
 */

/**
 * @brief Maximum number of application tasks.
 *
 * The kernel allocates one additional internal task slot for the idle task.
 * The total task capacity is therefore @c OS_MAX_TASKS + 1.
 */
#define OS_MAX_TASKS (3u)

/**
 * @brief Stack capacity allocated to each task, in 32-bit words.
 *
 * A value of 512 reserves 2048 bytes per task on the 32-bit Cortex-M port.
 * This capacity includes the initial exception frame and the task's complete
 * runtime stack usage.
 */
#define OS_TASK_STACK_SIZE (512u)

/** @} */

/**
 * @defgroup OS_CONFIG_PRIORITIES Task-priority configuration
 * @brief Scheduler priority range and ordering.
 * @{
 */

/**
 * @brief Number of distinct task-priority levels.
 *
 * The priority wait queue stores active priorities in a 32-bit bitmap, so
 * this value must be greater than zero and no greater than 32.
 */
#define OS_MAX_PRIORITIES (32u)

/**
 * @brief Lowest valid task priority.
 *
 * The idle task uses this priority but is managed separately from the normal
 * ready queue.
 */
#define OS_TASK_PRIORITY_LOWEST (0u)

/**
 * @brief Highest valid task priority.
 *
 * Application task priorities must be within the inclusive range
 * [@ref OS_TASK_PRIORITY_LOWEST, @ref OS_TASK_PRIORITY_HIGHEST].
 */
#define OS_TASK_PRIORITY_HIGHEST (OS_MAX_PRIORITIES - 1u)

/** @} */

/**
 * @defgroup OS_CONFIG_TRACING Trace configuration
 * @brief Master, backend, and event-category trace controls.
 * @{
 */

/**
 * @brief Master switch for all kernel tracing.
 *
 * When @c false, trace functions compile to no-op inline functions and no
 * trace backend should be required by the source code.
 */
#define OS_TRACE_ENABLED (true)

/**
 * @brief Enable the SEGGER SystemView trace backend.
 *
 * Requires the SEGGER SystemView sources, headers, and target configuration.
 * This option is meaningful only when @ref OS_TRACE_ENABLED is @c true.
 */
#define OS_TRACE_SEGGER_SYSVIEW (true)

/**
 * @brief Enable the Tessla-compatible text stream over SEGGER RTT.
 *
 * Requires the SEGGER RTT sources, headers, and target configuration. This
 * option is meaningful only when @ref OS_TRACE_ENABLED is @c true.
 */
#define OS_TRACE_TESSLA_RTT (true)

/**
 * @brief Trace scheduler decisions and task scheduling-state transitions.
 */
#define OS_TRACE_SCHEDULER (true)

/**
 * @brief Trace task creation and general task-state changes.
 */
#define OS_TRACE_TASKS (true)

/**
 * @brief Trace interrupt entry and exit events.
 */
#define OS_TRACE_ISR (true)

/**
 * @brief Trace mutex operations.
 */
#define OS_TRACE_MUTEX (false)

/**
 * @brief Trace semaphore operations.
 */
#define OS_TRACE_SEMAPHORE (true)

/**
 * @brief Trace message-queue operations.
 */
#define OS_TRACE_QUEUE (true)

/**
 * @brief Trace task-delay operations.
 */
#define OS_TRACE_DELAY (true)

/** @} */

/**
 * @defgroup OS_CONFIG_INTERRUPTS Cortex-M exception priorities
 * @brief Logical CMSIS priorities used by the kernel exceptions.
 *
 * Lower numeric values have greater hardware urgency on Cortex-M. These
 * values must fit within the implemented @c __NVIC_PRIO_BITS range.
 * @{
 */

/**
 * @brief Supervisor Call exception priority.
 *
 * SVC performs the initial transition from kernel startup to the first task.
 */
#define OS_SVC_INTERRUPT_PRIORITY (13u)

/**
 * @brief SysTick exception priority.
 *
 * SysTick advances the kernel time base, processes expired timeouts, and
 * requests round-robin scheduling.
 */
#define OS_SYSTICK_INTERRUPT_PRIORITY (14u)

/**
 * @brief PendSV exception priority.
 *
 * PendSV performs task context switches and should remain the kernel's least
 * urgent exception.
 */
#define OS_PENDSV_INTERRUPT_PRIORITY (15u)

/**
 * @brief BASEPRI threshold used by kernel critical sections.
 *
 * Interrupts at this logical priority and all numerically larger priorities
 * are masked while a kernel critical section is active. Priority zero is
 * invalid because BASEPRI cannot mask priority-zero interrupts.
 */
#define OS_KERNEL_INTERRUPT_PRIORITY OS_SVC_INTERRUPT_PRIORITY

/** @} */

#endif /* OS_CONFIG_H_ */