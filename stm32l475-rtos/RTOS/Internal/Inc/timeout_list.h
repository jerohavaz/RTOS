#ifndef TIMEOUT_LIST_H_
#define TIMEOUT_LIST_H_

#include "kernel_task.h"
#include "task_list.h"
#include <stdint.h>

typedef struct {
    task_list_t list;
} timeout_list_t;

void timeout_list_init(timeout_list_t *timeout_list);
void timeout_list_add(timeout_list_t *timeout_list, kernel_task_t *task, uint32_t wake_tick);
void timeout_list_remove(timeout_list_t *timeout_list, kernel_task_t *task);
kernel_task_t *timeout_list_pop_expired(timeout_list_t *timeout_list, uint32_t now);

#endif