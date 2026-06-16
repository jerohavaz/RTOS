/**
 * @file task_list.c
 * @brief Task list implementation.
 */

#include "task_list.h"

void task_list_init(task_list_t *list, task_node_fn_t get_node) {
    if ((list == 0) || (get_node == 0)) {
        return;
    }

    list->head = 0;
    list->count = 0u;
    list->get_node = get_node;
}

uint8_t task_list_is_empty(const task_list_t *list) {
    if (list == 0) {
        return 1u;
    }

    return (list->count == 0u);
}

void task_list_push_back(task_list_t *list, kernel_task_t *task) {
    if ((list == 0) || (task == 0) || (list->get_node == 0)) {
        return;
    }

    kernel_task_list_node_t *node = list->get_node(task);

    if (node == 0) {
        return;
    }

    node->next = 0;
    node->prev = 0;

    if (list->head == 0) {
        /* First element in a circular list points to itself. */
        list->head = task;
        node->next = task;
        node->prev = task;
        list->count = 1u;
        return;
    }

    kernel_task_t *head = list->head;
    kernel_task_list_node_t *head_node = list->get_node(head);

    if (head_node == 0) {
        return;
    }

    kernel_task_t *tail = head_node->prev;

    if (tail == 0) {
        return;
    }

    kernel_task_list_node_t *tail_node = list->get_node(tail);

    if (tail_node == 0) {
        return;
    }

    /* Insert before head, so the new task becomes the logical tail. */
    node->next = head;
    node->prev = tail;

    tail_node->next = task;
    head_node->prev = task;

    list->count++;
}

void task_list_insert_before(task_list_t *list, kernel_task_t *existing, kernel_task_t *task) {
    if ((list == 0) || (task == 0) || (list->get_node == 0)) {
        return;
    }

    if ((list->head == 0) || (existing == 0)) {
        task_list_push_back(list, task);
        return;
    }

    kernel_task_list_node_t *task_node = list->get_node(task);
    kernel_task_list_node_t *existing_node = list->get_node(existing);

    if ((task_node == 0) || (existing_node == 0)) {
        return;
    }

    kernel_task_t *prev = existing_node->prev;

    if (prev == 0) {
        return;
    }

    kernel_task_list_node_t *prev_node = list->get_node(prev);

    if (prev_node == 0) {
        return;
    }

    task_node->next = existing;
    task_node->prev = prev;

    prev_node->next = task;
    existing_node->prev = task;

    if (list->head == existing) {
        list->head = task;
    }

    list->count++;
}

kernel_task_t *task_list_pop_front(task_list_t *list) {
    if ((list == 0) || (list->head == 0)) {
        return 0;
    }

    kernel_task_t *task = list->head;
    task_list_remove(list, task);
    return task;
}

kernel_task_t *task_list_peek_front(const task_list_t *list) {
    if ((list == 0) || (list->head == 0)) {
        return 0;
    }

    return list->head;
}

void task_list_remove(task_list_t *list, kernel_task_t *task) {
    if ((list == 0) || (task == 0) || (list->get_node == 0)) {
        return;
    }

    if ((list->head == 0) || (list->count == 0u)) {
        return;
    }

    kernel_task_list_node_t *node = list->get_node(task);

    if (node == 0) {
        return;
    }

    if (list->count == 1u) {
        if (list->head != task) {
            return;
        }

        list->head = 0;
        list->count = 0u;
        node->next = 0;
        node->prev = 0;
        return;
    }

    kernel_task_t *next = node->next;
    kernel_task_t *prev = node->prev;

    if ((next == 0) || (prev == 0)) {
        return;
    }

    kernel_task_list_node_t *next_node = list->get_node(next);
    kernel_task_list_node_t *prev_node = list->get_node(prev);

    if ((next_node == 0) || (prev_node == 0)) {
        return;
    }

    prev_node->next = next;
    next_node->prev = prev;

    if (list->head == task) {
        list->head = next;
    }

    node->next = 0;
    node->prev = 0;

    list->count--;
}