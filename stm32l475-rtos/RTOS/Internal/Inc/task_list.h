/**
 * @file task_list.h
 * @brief Intrusive circular doubly linked list for kernel tasks.
 *
 * The list uses a node embedded in each task object. A task can therefore be
 * linked into different lists by using different embedded nodes.
 *
 * @note This module does not allocate memory.
 * @note Functions are not internally synchronized.
 * @note Protect shared lists with a scheduler lock, critical section, or mutex.
 */
#ifndef TASK_LIST_H_
#define TASK_LIST_H_

#include "kernel_task.h"

#include <stdint.h>

/**
 * @brief Callback used to select the embedded list node of a task.
 *
 * @param task Task whose embedded node shall be returned.
 *
 * @return Pointer to the selected embedded list node.
 * @retval 0 Invalid task or unavailable node.
 */
typedef kernel_task_list_node_t *(*task_node_fn_t)(kernel_task_t *task);

/**
 * @brief Intrusive task list.
 */
typedef struct {
    kernel_task_t *head;     ///< First task in the list, or 0 if empty.
    uint32_t count;          ///< Number of tasks in the list.
    task_node_fn_t get_node; ///< Callback used to access the embedded node.
} task_list_t;

/**
 * @brief Initialize a task list.
 *
 * @param list List to initialize.
 * @param get_node Callback used to retrieve the embedded task node.
 */
void task_list_init(task_list_t *list, task_node_fn_t get_node);

/**
 * @brief Check whether a task list is empty.
 *
 * @param list List to inspect.
 *
 * @retval 1 List is empty or invalid.
 * @retval 0 List contains at least one task.
 */
uint8_t task_list_is_empty(const task_list_t *list);

/**
 * @brief Append a task to the back of the list.
 *
 * @param list List to modify.
 * @param task Task to append.
 *
 * @pre list must be initialized with task_list_init().
 * @pre task must not already be linked through the selected node.
 */
void task_list_push_back(task_list_t *list, kernel_task_t *task);

/**
 * @brief Insert a task before an existing task.
 *
 * If @p existing is the head, @p task becomes the new head. If the list is
 * empty or @p existing is 0, this falls back to task_list_push_back().
 *
 * @param list List to modify.
 * @param existing Existing task before which @p task is inserted.
 * @param task Task to insert.
 *
 * @pre list must be initialized with task_list_init().
 * @pre task must not already be linked through the selected node.
 * @pre existing must belong to @p list unless it is 0.
 */
void task_list_insert_before(task_list_t *list, kernel_task_t *existing, kernel_task_t *task);

/**
 * @brief Remove and return the front task.
 *
 * @param list List to modify.
 *
 * @return Removed front task.
 * @retval 0 List is empty or invalid.
 */
kernel_task_t *task_list_pop_front(task_list_t *list);

/**
 * @brief Return the front task without removing it.
 *
 * @param list List to inspect.
 *
 * @return Front task.
 * @retval 0 List is empty or invalid.
 */
kernel_task_t *task_list_peek_front(const task_list_t *list);

/**
 * @brief Remove a task from the list.
 *
 * @param list List to modify.
 * @param task Task to remove.
 *
 * @pre list must be initialized with task_list_init().
 * @pre task must belong to @p list.
 *
 * @warning Passing a task that does not belong to @p list can corrupt lists.
 */
void task_list_remove(task_list_t *list, kernel_task_t *task);

#endif /* TASK_LIST_H_ */