#include "task_list.h"

void task_list_init(task_list_t *list, task_node_fn_t get_node) {
    list->head = 0;
    list->count = 0u;
    list->node = get_node;
}

uint8_t task_list_is_empty(const task_list_t *list) {
    return list->count == 0u;
}

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

tcb_t *task_list_pop_front(task_list_t *list) {
    if (list->head == 0) {
        return 0;
    }

    tcb_t *task = list->head;
    task_list_remove(list, task);
    return task;
}

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