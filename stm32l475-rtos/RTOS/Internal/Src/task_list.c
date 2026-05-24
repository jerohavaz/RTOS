/**
 * @file task_list.c
 * @brief Intrusive circular doubly linked list for RTOS task control blocks.
 *
 * This module manages lists of TCBs using a list node embedded inside each
 * task control block. The list is circular: the head node's previous pointer
 * points to the tail, and the tail node's next pointer points to the head.
 *
 * @note This module does not allocate memory.
 * @note The caller must provide valid TCB pointers.
 * @note These functions are not internally synchronized. Protect calls with
 *       the scheduler lock, critical section, or RTOS mutex when shared.
 */

#include "task_list.h"
#include "tcb.h"

/**
 * @brief Initialize a task list.
 *
 * Sets the list to empty and stores the callback used to retrieve the embedded
 * list node from a TCB.
 *
 * @param list List instance to initialize.
 * @param get_node Function used to get the list node embedded in a TCB.
 */
void task_list_init(task_list_t *list, task_node_fn_t get_node) {
    list->head = 0;
    list->count = 0u;
    list->node = get_node;
}

/**
 * @brief Check whether a task list is empty.
 *
 * @param list List to inspect.
 *
 * @retval 1 List contains no tasks.
 * @retval 0 List contains at least one task.
 */
uint8_t task_list_is_empty(const task_list_t *list) {
    return list->count == 0u;
}

/**
 * @brief Append a task to the back of the list.
 *
 * Inserts @p task before the current head, making it the logical tail.
 * If the list is empty, @p task becomes the head and points to itself.
 *
 * @param list List to modify.
 * @param task Task control block to append.
 */
void task_list_push_back(task_list_t *list, tcb_t *task) {
    tcb_list_node_t *node = list->node(task);

    node->next = 0;
    node->prev = 0;

    if (list->head == 0) {
        list->head = task;
        node->next = task;
        node->prev = task;
        list->count = 1u;
        return;
    }

    tcb_t *head = list->head;
    tcb_t *tail = list->node(head)->prev;

    tcb_list_node_t *head_node = list->node(head);
    tcb_list_node_t *tail_node = list->node(tail);

    node->next = head;
    node->prev = tail;

    tail_node->next = task;
    head_node->prev = task;

    list->count++;
}

/**
 * @brief Insert a task before an existing task.
 *
 * Inserts @p task directly before @p existing. If @p existing is the current
 * head, @p task becomes the new head. If the list is empty or @p existing is
 * null, this falls back to appending @p task to the back.
 *
 * @param list List to modify.
 * @param existing Task already in the list, before which @p task is inserted.
 * @param task Task control block to insert.
 */
void task_list_insert_before(task_list_t *list, tcb_t *existing, tcb_t *task) {
    if ((list->head == 0) || (existing == 0)) {
        task_list_push_back(list, task);
        return;
    }

    tcb_list_node_t *task_node = list->node(task);
    tcb_list_node_t *existing_node = list->node(existing);

    tcb_t *prev = existing_node->prev;
    tcb_list_node_t *prev_node = list->node(prev);

    task_node->next = existing;
    task_node->prev = prev;

    prev_node->next = task;
    existing_node->prev = task;

    if (list->head == existing) {
        list->head = task;
    }

    list->count++;
}

/**
 * @brief Remove and return the front task.
 *
 * Removes the current head from the list. The next task becomes the new head.
 *
 * @param list List to modify.
 *
 * @return Removed task control block.
 * @retval 0 List was empty.
 */
tcb_t *task_list_pop_front(task_list_t *list) {
    if (list->head == 0) {
        return 0;
    }

    tcb_t *task = list->head;
    task_list_remove(list, task);
    return task;
}

/**
 * @brief Return the front task without removing it.
 *
 * Returns the current head of the list. The list is not modified.
 *
 * @param list List to inspect.
 *
 * @return Front task control block.
 * @retval 0 List was empty.
 */
tcb_t *task_list_peek_front(const task_list_t *list) {
    if (list->head == 0) {
        return 0;
    }

    return list->head;
}

/**
 * @brief Remove a task from the list.
 *
 * Unlinks @p task from the circular list and clears its node links.
 *
 * @param list List to modify.
 * @param task Task control block to remove.
 *
 * @warning This function assumes @p task belongs to @p list. Passing a task
 *          that is not in the list can corrupt the list.
 */
void task_list_remove(task_list_t *list, tcb_t *task) {
    tcb_list_node_t *node = list->node(task);

    if ((list->head == 0) || (list->count == 0u)) {
        return;
    }

    if (list->count == 1u) {
        list->head = 0;
        list->count = 0u;
        node->next = 0;
        node->prev = 0;
        return;
    }

    tcb_t *next = node->next;
    tcb_t *prev = node->prev;

    list->node(prev)->next = next;
    list->node(next)->prev = prev;

    if (list->head == task) {
        list->head = next;
    }

    node->next = 0;
    node->prev = 0;

    list->count--;
}