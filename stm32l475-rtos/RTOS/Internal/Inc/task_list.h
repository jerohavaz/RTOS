/**
 * @file task_list.h
 * @brief Intrusive circular doubly linked list for kernel tasks.
 * @author Jerome
 *
 * @details
 * Each list stores tasks through a caller-selected
 * @ref kernel_task_list_node_t embedded in the task object. A task may be
 * linked into multiple lists simultaneously only when each list uses a
 * different embedded node.
 *
 * The list is circular: the tail points to the head and the head's previous
 * link identifies the tail. All operations are O(1); the implementation never
 * scans the list, allocates memory, or performs internal synchronization.
 * Callers must protect shared lists with the appropriate kernel critical
 * section.
 */
#ifndef TASK_LIST_H_
#define TASK_LIST_H_

#include "kernel_task.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Callback used to select an embedded list node from a task.
 *
 * @param task Task whose embedded node is requested.
 *
 * @return Pointer to the selected embedded list node.
 *
 * @pre @p task must not be null.
 * @post The returned pointer must not be null.
 *
 * @note A list stores this callback during @ref task_list_init and uses it for
 *       every task subsequently passed to that list.
 */
typedef kernel_task_list_node_t *(*task_node_fn_t)(kernel_task_t *task);

/**
 * @brief Intrusive task list.
 *
 * @invariant An empty list has @ref head equal to null and @ref count equal to
 *            zero.
 * @invariant A non-empty list has a non-null @ref head and a positive
 *            @ref count.
 * @invariant For a one-element list, the selected node's next and previous
 *            pointers both reference the head task.
 */
typedef struct {
    kernel_task_t *head;     /**< First task, or null when the list is empty. */
    uint32_t count;          /**< Number of tasks currently linked in the list. */
    task_node_fn_t get_node; /**< Selector for this list's embedded task node. */
} task_list_t;

/**
 * @brief Initialize a task list.
 *
 * @param list List to initialize.
 * @param get_node Callback used to retrieve this list's embedded task node.
 *
 * @pre @p list must not be null.
 * @pre @p get_node must not be null.
 * @pre @p list must not own linked tasks; reinitialization does not unlink
 *      existing nodes.
 * @post The list is empty and retains @p get_node for later operations.
 *
 * @note Time complexity is O(1).
 */
void task_list_init(task_list_t *list, task_node_fn_t get_node);

/**
 * @brief Check whether a task list is empty.
 *
 * @param list List to inspect.
 *
 * @retval true The list contains no tasks.
 * @retval false The list contains at least one task.
 *
 * @pre @p list must not be null.
 *
 * @note The implementation checks consistency between @ref task_list_t::head
 *       and @ref task_list_t::count.
 * @note Time complexity is O(1).
 */
bool task_list_is_empty(const task_list_t *list);

/**
 * @brief Append a task to the back of the list.
 *
 * @param list List to modify.
 * @param task Task to append.
 *
 * @pre @p list must be initialized with @ref task_list_init.
 * @pre @p task must not be null.
 * @pre The selected node in @p task must be unlinked.
 * @post @p task is the logical tail; the existing head is unchanged.
 *
 * @note In an empty list, @p task becomes the head and its selected next and
 *       previous links point to itself.
 * @note Time complexity is O(1).
 */
void task_list_push_back(task_list_t *list, kernel_task_t *task);

/**
 * @brief Insert a task before an existing task.
 *
 * If @p existing is the current head, @p task becomes the new head. If the
 * list is empty or @p existing is null, this function behaves as
 * @ref task_list_push_back.
 *
 * @param list List to modify.
 * @param existing Existing task before which @p task is inserted, or null to
 *                 append.
 * @param task Task to insert.
 *
 * @pre @p list must be initialized with @ref task_list_init.
 * @pre @p task must not be null.
 * @pre The selected node in @p task must be unlinked.
 * @pre @p existing must be null or linked in @p list through the selected
 *      node.
 *
 * @note Time complexity is O(1).
 */
void task_list_insert_before(task_list_t *list, kernel_task_t *existing, kernel_task_t *task);

/**
 * @brief Remove and return the front task.
 *
 * @param list List to modify.
 *
 * @return Removed front task.
 * @retval NULL The list is empty.
 *
 * @pre @p list must be initialized with @ref task_list_init.
 * @post A removed task's selected next and previous pointers are null.
 *
 * @note If another task remains, it becomes the new head.
 * @note Time complexity is O(1).
 */
kernel_task_t *task_list_pop_front(task_list_t *list);

/**
 * @brief Return the front task without removing it.
 *
 * @param list List to inspect.
 *
 * @return Current front task.
 * @retval NULL The list is empty.
 *
 * @pre @p list must not be null.
 *
 * @note This function does not modify the list.
 * @note Time complexity is O(1).
 */
kernel_task_t *task_list_peek_front(const task_list_t *list);

/**
 * @brief Remove a task that must be present in the list.
 *
 * @param list List to modify.
 * @param task Task to remove.
 *
 * @pre @p list must be initialized with @ref task_list_init.
 * @pre @p task must not be null.
 * @pre @p task must be linked in @p list through the selected node.
 * @post The removed task's selected next and previous pointers are null.
 * @post If the removed task was the head, its successor becomes the new head.
 *
 * @note Violating the membership precondition triggers a kernel panic.
 * @note Time complexity is O(1).
 */
void task_list_remove(task_list_t *list, kernel_task_t *task);

/**
 * @brief Remove a task if it is currently linked through this list's selected node.
 *
 * @param list List to modify.
 * @param task Task to remove if present.
 *
 * @retval true The selected node was linked and the task was removed.
 * @retval false The list was empty or the selected node was unlinked.
 *
 * @pre @p list must be initialized with @ref task_list_init.
 * @pre @p task must not be null.
 * @post On successful removal, the selected next and previous pointers are
 *       null.
 *
 * @warning The function can detect whether the selected node is linked, but it
 *          cannot prove that the node belongs to @p list. The selected node
 *          must be exclusive to this list domain; otherwise attempting removal
 *          can corrupt both lists and trigger a kernel panic.
 *
 * @note Time complexity is O(1).
 */
bool task_list_try_remove(task_list_t *list, kernel_task_t *task);

#endif /* TASK_LIST_H_ */