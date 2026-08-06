/**
 * @file prio_waitq.h
 * @brief Internal fixed-priority task wait queue.
 * @author Jerome
 *
 * @details
 * Implements a priority-indexed collection of intrusive task lists. Each
 * priority owns one FIFO list, while a 32-bit bitmap records which priority
 * lists are non-empty. The highest numeric task priority is selected first;
 * tasks sharing a priority are returned in insertion order.
 *
 * The queue does not allocate memory and does not provide internal
 * synchronization. Callers must protect shared queue state with the
 * appropriate kernel critical section.
 *
 * @note @c OS_MAX_PRIORITIES must be in the range 1 through 32 because one
 *       bit in @ref prio_waitq_t::bitmap represents each priority.
 */

#ifndef PRIO_WAITQ_H_
#define PRIO_WAITQ_H_

#include "kernel_task.h"
#include "os_config.h"
#include "task_list.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Priority-ordered intrusive task queue.
 *
 * All per-priority lists use the same node-selector callback supplied to
 * @ref prio_waitq_init. A task can be linked in this queue only when that
 * selected embedded node is not already linked in another list.
 *
 * The following invariant must always hold for every valid priority @c p:
 * bit @c p in @ref bitmap is set exactly when @c prio[p] is non-empty.
 */
typedef struct {
    /**
     * @brief FIFO task list for each numeric priority.
     *
     * Index zero represents the lowest priority; index
     * @c OS_MAX_PRIORITIES-1 represents the highest.
     */
    task_list_t prio[OS_MAX_PRIORITIES];

    /**
     * @brief Non-empty priority bitmap.
     *
     * Bit @c p is set when @ref prio contains at least one task at priority
     * @c p. The most significant set bit identifies the highest runnable or
     * waiting priority.
     */
    uint32_t bitmap;
} prio_waitq_t;

/**
 * @brief Initialize an empty priority wait queue.
 *
 * Clears the priority bitmap and initializes every per-priority list with the
 * supplied task-node selector.
 *
 * @param q Queue to initialize.
 * @param get_node Callback that returns the intrusive node to use from each
 *                 task.
 *
 * @pre @p q must not be null.
 * @pre @p get_node must not be null.
 * @post @ref prio_waitq_is_empty returns @c true.
 *
 * @note Time complexity is O(@c OS_MAX_PRIORITIES).
 */
void prio_waitq_init(prio_waitq_t *q, task_node_fn_t get_node);

/**
 * @brief Append a task to its priority's FIFO list.
 *
 * Uses @c task->tcb.u8TaskPrio as the destination index and sets the
 * corresponding bitmap bit.
 *
 * @param q Initialized queue.
 * @param task Task to enqueue.
 *
 * @pre @p q must not be null.
 * @pre @p task must not be null.
 * @pre The task priority must be less than @c OS_MAX_PRIORITIES.
 * @pre The selected intrusive task node must not already be linked.
 *
 * @note Time complexity is O(1).
 */
void prio_waitq_push(prio_waitq_t *q, kernel_task_t *task);

/**
 * @brief Remove and return the oldest task at the highest active priority.
 *
 * Finds the most significant set bitmap bit, removes the front task from that
 * priority's FIFO list, and clears the bit if the list becomes empty.
 *
 * @param q Initialized queue.
 *
 * @return Removed task.
 * @retval NULL The queue is empty.
 *
 * @pre @p q must not be null.
 *
 * @note Time complexity is O(1) in the current 32-bit bitmap implementation.
 */
kernel_task_t *prio_waitq_pop_highest(prio_waitq_t *q);

/**
 * @brief Return the oldest task at the highest active priority without removal.
 *
 * @param q Initialized queue.
 *
 * @return Highest-priority front task.
 * @retval NULL The queue is empty.
 *
 * @pre @p q must not be null.
 *
 * @note This function does not modify list membership or the bitmap.
 * @note Time complexity is O(1) in the current 32-bit bitmap implementation.
 */
kernel_task_t *prio_waitq_peek_highest(prio_waitq_t *q);

/**
 * @brief Remove a specific task from its priority list.
 *
 * Selects the list using @c task->tcb.u8TaskPrio and clears the corresponding
 * bitmap bit if removal leaves that list empty.
 *
 * @param q Initialized queue containing @p task.
 * @param task Task to remove.
 *
 * @pre @p q must not be null.
 * @pre @p task must not be null.
 * @pre The task priority must be less than @c OS_MAX_PRIORITIES.
 * @pre The task must be linked in this queue through the configured node.
 *
 * @note Time complexity is O(1).
 */
void prio_waitq_remove(prio_waitq_t *q, kernel_task_t *task);

/**
 * @brief Test whether the queue contains no tasks.
 *
 * @param q Initialized queue to inspect.
 *
 * @retval true No priority bit is set.
 * @retval false At least one priority contains a task.
 *
 * @pre @p q must not be null.
 *
 * @note This check relies on the bitmap/list invariant.
 * @note Time complexity is O(1).
 */
bool prio_waitq_is_empty(const prio_waitq_t *q);

#endif /* PRIO_WAITQ_H_ */