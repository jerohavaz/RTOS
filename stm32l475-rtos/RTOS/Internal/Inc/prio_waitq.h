#ifndef PRIO_WAITQ_H_
#define PRIO_WAITQ_H_

#include "kernel_task.h"
#include "task_list.h"
#include "os_config.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    task_list_t prio[OS_MAX_PRIORITIES];
    uint32_t bitmap;
} prio_waitq_t;

void prio_waitq_init(prio_waitq_t *q, task_node_fn_t get_node);

void prio_waitq_push(prio_waitq_t *q, kernel_task_t *task);
kernel_task_t *prio_waitq_pop_highest(prio_waitq_t *q);
kernel_task_t *prio_waitq_peek_highest(prio_waitq_t *q);
void prio_waitq_remove(prio_waitq_t *q, kernel_task_t *task);

bool prio_waitq_is_empty(const prio_waitq_t *q);

#endif