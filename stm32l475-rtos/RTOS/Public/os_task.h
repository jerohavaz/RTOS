/**
 * @file os_task.h
 * @brief Static RTOS task creation.
 * @author Jerome
 *
 * Provides task creation from the kernel's fixed-size task table. Tasks must
 * be created before os_start() locks further creation.
 */
#ifndef OS_TASK_H_
#define OS_TASK_H_

#include "os_types.h"
#include <stdint.h>

/**
 * @brief Task entry-function type.
 *
 * Task entry functions take no arguments and return no value.
 *
 * @note A task entry function must not return.
 */
typedef void (*os_task_func_t)(void);

/**
 * @brief Create a task in the kernel's static task table.
 *
 * Allocates the next task slot, constructs its initial CPU context, and leaves
 * it in the created state. The scheduler makes created tasks ready when
 * os_start() is called.
 *
 * @param task_func Non-returning task entry function.
 * @param prio Task priority. Larger numeric values represent higher priority.
 *
 * @retval OS_OK Task created successfully.
 * @retval OS_ERR_NULL @p task_func is 0.
 * @retval OS_ERR_INVALID_STATE Task creation is locked or initial stack
 *         construction failed.
 * @retval OS_ERR_INVALID_PRIO @p prio exceeds
 *         @ref OS_TASK_PRIORITY_HIGHEST.
 * @retval OS_ERR_FULL All configured application-task slots are in use.
 *
 * @pre The RTOS must be initialized with os_init() and not yet started with
 *      os_start().
 * @note Task stacks and control blocks are statically allocated by the kernel.
 */
os_status_t os_task_create(os_task_func_t task_func, uint8_t prio);

#endif /* OS_TASK_H_ */