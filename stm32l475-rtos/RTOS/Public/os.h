/**
 * @file os.h
 * @brief Core RTOS public interface and lifecycle control.
 * @author Jerome
 *
 * Includes the common status, task, delay, and interrupt-hook interfaces and
 * declares kernel initialization and startup.
 */
#ifndef OS_H_
#define OS_H_

#include "os_types.h"
#include "os_task.h"
#include "os_delay.h"
#include "os_isr.h"

/**
 * @brief Initialize the RTOS kernel.
 *
 * Disables normal interrupts, initializes tracing, timeout and task state,
 * initializes the scheduler and its port interrupts, and creates the dedicated
 * idle task.
 *
 * @post The kernel is initialized but scheduling has not started.
 * @post Application tasks may be created with os_task_create().
 * @post Normal interrupts remain disabled until scheduler startup.
 *
 * @note Call this function once before using other RTOS services.
 */
void os_init(void);

/**
 * @brief Lock task creation and start the scheduler.
 *
 * Makes all created tasks ready, selects the highest-priority task, and
 * transfers control through the architecture-specific first-task startup path.
 *
 * @pre os_init() must have completed.
 * @pre All application tasks must already have been created.
 *
 * @note This function does not return during normal operation. An unexpected
 *       return from the port startup path triggers a kernel panic.
 */
void os_start(void);

#endif /* OS_H_ */