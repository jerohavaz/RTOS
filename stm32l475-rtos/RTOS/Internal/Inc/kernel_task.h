#ifndef KERNEL_TASK_H_
#define KERNEL_TASK_H_

#include "os_types.h"
#include "tcb.h"
#include <stdint.h>

typedef struct kernel_task kernel_task_t;

typedef struct {
    kernel_task_t *next;
    kernel_task_t *prev;
} kernel_task_list_node_t;

struct kernel_task {
    TCB_sctTCB_t tcb;

    kernel_task_list_node_t sched_node;
    kernel_task_list_node_t timeout_node;

    uint32_t wake_tick;
    void *wait_object;
    os_status_t wait_result;
};

#endif