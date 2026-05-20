#ifndef TASK_LIST_H_
#define TASK_LIST_H_

#include "tcb.h"
#include <stdint.h>

typedef tcb_list_node_t *(*task_node_fn_t)(tcb_t *task);

typedef struct {
    tcb_t *head;
    uint32_t count;
    task_node_fn_t node;
} task_list_t;

void task_list_init(task_list_t *list, task_node_fn_t node);
uint8_t task_list_is_empty(const task_list_t *list);

void task_list_push_back(task_list_t *list, tcb_t *task);
void task_list_insert_before(task_list_t *list, tcb_t *existing, tcb_t *task);
tcb_t *task_list_pop_front(task_list_t *list);
void task_list_remove(task_list_t *list, tcb_t *task);

#endif