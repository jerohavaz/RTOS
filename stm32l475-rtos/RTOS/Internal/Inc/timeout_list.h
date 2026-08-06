/**
 * @file timeout_list.h
 * @brief Ordered list of kernel tasks with finite wake deadlines.
 * @author Jerome
 *
 * @details
 * Maintains tasks in ascending wrap-safe wake order using each task's
 * @ref kernel_task::timeout_node and @ref kernel_task::wake_tick fields. Tasks
 * with identical deadlines retain insertion order.
 *
 * Deadline comparisons use signed 32-bit tick differences. This ordering is
 * valid only when every scheduled delay is shorter than 2^31 ticks, matching
 * the kernel's @c K_TIMEOUT_MAX contract.
 *
 * The module does not allocate memory or provide internal synchronization.
 * Callers must protect the list with the appropriate kernel critical section.
 */

#ifndef TIMEOUT_LIST_H_
#define TIMEOUT_LIST_H_

#include "kernel_task.h"
#include "task_list.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Ordered timeout queue.
 *
 * Wraps an intrusive task list configured to select
 * @ref kernel_task::timeout_node. The logical head always contains the earliest
 * deadline under the signed-difference ordering rule.
 */
typedef struct {
    task_list_t list; /**< Intrusive list ordered by task wake deadline. */
} timeout_list_t;

/**
 * @brief Initialize an empty timeout list.
 *
 * Configures the embedded task list to use each task's dedicated timeout node.
 *
 * @param timeout_list Timeout list to initialize.
 *
 * @pre @p timeout_list must not be null.
 * @pre The list must not own linked tasks; reinitialization does not unlink
 *      existing timeout nodes.
 * @post The timeout list is empty.
 *
 * @note Time complexity is O(1).
 */
void timeout_list_init(timeout_list_t *timeout_list);

/**
 * @brief Insert a task at its ordered wake position.
 *
 * Stores @p wake_tick in the task and inserts it before the first task whose
 * deadline is later. If no later deadline exists, the task is appended. A task
 * added after existing tasks with the same deadline remains behind them.
 *
 * @param timeout_list Initialized timeout list.
 * @param task Task to register for timeout processing.
 * @param wake_tick Absolute 32-bit kernel tick at which the wait expires.
 *
 * @pre @p timeout_list must not be null.
 * @pre @p task must not be null.
 * @pre @c task->wake_tick must be zero.
 * @pre @c task->timeout_node must be unlinked.
 * @pre The deadline must originate from a delay shorter than 2^31 ticks.
 * @post @p task is linked in deadline order through its timeout node.
 *
 * @note @p wake_tick may equal zero after natural tick-counter wrap-around.
 * @note Time complexity is O(n), where n is the number of registered tasks.
 */
void timeout_list_add(timeout_list_t *timeout_list, kernel_task_t *task, uint32_t wake_tick);

/**
 * @brief Remove a task that must be registered in the timeout list.
 *
 * Unlinks the task's timeout node and resets its wake tick to zero.
 *
 * @param timeout_list Initialized timeout list containing @p task.
 * @param task Registered task to remove.
 *
 * @pre @p timeout_list must not be null.
 * @pre @p task must not be null.
 * @pre @p task must be linked in @p timeout_list through its timeout node.
 * @post The timeout node is unlinked and @c task->wake_tick is zero.
 *
 * @note Violating the membership precondition triggers a kernel panic.
 * @note Time complexity is O(1).
 */
void timeout_list_remove(timeout_list_t *timeout_list, kernel_task_t *task);

/**
 * @brief Remove a task if its timeout node is currently linked.
 *
 * @param timeout_list Initialized timeout list.
 * @param task Task to remove if registered.
 *
 * @retval true The task was removed and its wake tick was reset.
 * @retval false The list was empty or the task's timeout node was unlinked.
 *
 * @pre @p timeout_list must not be null.
 * @pre @p task must not be null.
 *
 * @warning As with @ref task_list_try_remove, linkage alone cannot prove list
 *          ownership. A task's timeout node must be used exclusively by this
 *          timeout-list domain.
 *
 * @note Time complexity is O(1).
 */
bool timeout_list_try_remove(timeout_list_t *timeout_list, kernel_task_t *task);

/**
 * @brief Remove and return the earliest task if its deadline has expired.
 *
 * Compares @p now with the head task using signed tick subtraction. At most one
 * task is removed per call; callers should repeat the call to drain all tasks
 * that expired at or before @p now.
 *
 * @param timeout_list Initialized timeout list.
 * @param now Current 32-bit kernel tick.
 *
 * @return Removed expired task.
 * @retval NULL The list is empty or its earliest deadline is still in the
 *              future.
 *
 * @pre @p timeout_list must not be null.
 * @pre All registered deadlines must satisfy the less-than-2^31-ticks ordering
 *      contract.
 * @post A returned task has an unlinked timeout node and a zero wake tick.
 *
 * @note Time complexity is O(1).
 */
kernel_task_t *timeout_list_pop_expired(timeout_list_t *timeout_list, uint32_t now);

#endif /* TIMEOUT_LIST_H_ */