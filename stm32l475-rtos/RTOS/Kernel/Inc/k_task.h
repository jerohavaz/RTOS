/**
 * @file k_task.h
 * @brief Internal task storage and creation interface.
 * @author Jerome
 *
 * Manages the statically allocated kernel task table. The table contains the
 * configured application-task slots plus one slot reserved for the idle task.
 */
#ifndef K_TASK_H_
#define K_TASK_H_

#include "os_types.h"
#include "os_task.h"
#include "kernel_task.h"
#include <stdint.h>

/** @brief Total number of kernel task slots, including the idle task. */
#define K_MAX_TASKS OS_MAX_TASKS + 1u

/**
 * @brief Initialize the kernel task table.
 *
 * Clears every task slot, resets the task count, and permits task creation.
 * This function is intended for kernel startup before tasks are created.
 */
void k_task_init(void);

/**
 * @brief Permanently disable further task creation for the current run.
 *
 * @pre Task creation must not already be locked.
 *
 * @note Calling k_task_init() resets the lock as part of kernel initialization.
 */
void k_task_lock_creation(void);

/**
 * @brief Create a task in the static kernel task table.
 *
 * Initializes the next free task slot, constructs its initial port stack
 * frame, assigns its task ID and priority, and emits a task-creation trace.
 * The task is left in @ref TaskState_Created; this function does not make it
 * ready to run.
 *
 * @param task_func Entry function executed by the task.
 * @param prio Task priority in the configured valid priority range.
 * @param[out] out_task Receives the created task, or 0 if creation fails.
 *
 * @retval OS_OK Task created successfully.
 * @retval OS_ERR_NULL @p task_func or @p out_task is 0.
 * @retval OS_ERR_INVALID_STATE Creation is locked or stack initialization
 *         failed.
 * @retval OS_ERR_INVALID_PRIO @p prio exceeds
 *         @ref OS_TASK_PRIORITY_HIGHEST.
 * @retval OS_ERR_FULL No task slot remains.
 *
 * @note This internal function is also used to create the idle task.
 */
os_status_t k_task_create_internal(os_task_func_t task_func,
                                   uint8_t prio,
                                   kernel_task_t **out_task);

/**
 * @brief Return a created task by table index.
 *
 * Task IDs are assigned from the same zero-based table index.
 *
 * @param index Zero-based task table index.
 *
 * @return Pointer to the task, or 0 if @p index is not below the current task
 *         count.
 */
kernel_task_t *k_task_get(uint32_t index);

/**
 * @brief Return the number of successfully created tasks.
 *
 * @return Current task count, including the idle task after it is created.
 */
uint32_t k_task_count(void);

#endif /* K_TASK_H_ */