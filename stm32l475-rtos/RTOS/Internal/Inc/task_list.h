/**
 * @file task_list.h
 * @brief Intrusive circular doubly linked list for kernel tasks.
 *
 * Each list stores tasks through a caller-selected node embedded in the task
 * object. A task may be linked into multiple lists at the same time only if
 * each list uses a different embedded node.
 *
 * This module does not allocate memory and does not perform internal
 * synchronization. Shared lists must be protected by the caller.
 */
#ifndef TASK_LIST_H_
#define TASK_LIST_H_

#include "kernel_task.h"

#include <stdint.h>

/**
 * @brief Callback used to select an embedded list node from a task.
 *
 * @param task Task whose embedded node is requested.
 *
 * @return Pointer to the selected embedded list node.
 *
 * @pre task must not be 0.
 */
typedef kernel_task_list_node_t *(*task_node_fn_t)(kernel_task_t *task);

/**
 * @brief Intrusive task list.
 */
typedef struct {
    kernel_task_t *head;     ///< First task in the list, or 0 if the list is empty.
    uint32_t count;          ///< Number of tasks currently linked in the list.
    task_node_fn_t get_node; ///< Callback used to access this list's embedded node.
} task_list_t;

/**
 * @brief Initialize a task list.
 *
 * @param list List to initialize.
 * @param get_node Callback used to retrieve this list's embedded task node.
 *
 * @pre list must not be 0.
 * @pre get_node must not be 0.
 */
void task_list_init(task_list_t *list, task_node_fn_t get_node);

/**
 * @brief Check whether a task list is empty.
 *
 * @param list List to inspect.
 *
 * @return 1 if the list is empty, otherwise 0.
 *
 * @pre list must not be 0.
 */
uint8_t task_list_is_empty(const task_list_t *list);

/**
 * @brief Append a task to the back of the list.
 *
 * @param list List to modify.
 * @param task Task to append.
 *
 * @pre list must be initialized with task_list_init().
 * @pre task must not be 0.
 * @pre task must not already be linked through this list's selected node.
 */
void task_list_push_back(task_list_t *list, kernel_task_t *task);

/**
 * @brief Insert a task before an existing task.
 *
 * If @p existing is the current head, @p task becomes the new head. If the list
 * is empty or @p existing is 0, this function appends @p task to the back of the
 * list.
 *
 * @param list List to modify.
 * @param existing Existing task before which @p task is inserted, or 0.
 * @param task Task to insert.
 *
 * @pre list must be initialized with task_list_init().
 * @pre task must not be 0.
 * @pre task must not already be linked through this list's selected node.
 * @pre existing must be 0 or must already be linked in @p list.
 */
void task_list_insert_before(task_list_t *list, kernel_task_t *existing, kernel_task_t *task);

/**
 * @brief Remove and return the front task.
 *
 * @param list List to modify.
 *
 * @return Removed front task, or 0 if the list is empty.
 *
 * @pre list must be initialized with task_list_init().
 */
kernel_task_t *task_list_pop_front(task_list_t *list);

/**
 * @brief Return the front task without removing it.
 *
 * @param list List to inspect.
 *
 * @return Front task, or 0 if the list is empty.
 *
 * @pre list must not be 0.
 */
kernel_task_t *task_list_peek_front(const task_list_t *list);

/**
 * @brief Remove a task that must be present in the list.
 *
 * @param list List to modify.
 * @param task Task to remove.
 *
 * @pre list must be initialized with task_list_init().
 * @pre task must not be 0.
 * @pre task must be linked in @p list through this list's selected node.
 */
void task_list_remove(task_list_t *list, kernel_task_t *task);

/**
 * @brief Remove a task if it is currently linked through this list's selected node.
 *
 * @param list List to modify.
 * @param task Task to remove if present.
 *
 * @return 1 if the task was removed, otherwise 0.
 *
 * @pre list must be initialized with task_list_init().
 * @pre task must not be 0.
 *
 * @warning This function can only prove that @p task is linked through this
 * list's selected node. It assumes that the selected node is used exclusively
 * by this list domain.
 */
uint8_t task_list_try_remove(task_list_t *list, kernel_task_t *task);

#endif /* TASK_LIST_H_ */