#ifndef KERNEL_TASK_H_
#define KERNEL_TASK_H_

#include "os_types.h"
#include "tcb.h"
#include <stdint.h>

typedef enum {
    K_WAIT_NONE = 0,
    K_WAIT_DELAY,
    K_WAIT_SEM,
    K_WAIT_MUTEX,
    K_WAIT_QUEUE_SEND,
    K_WAIT_QUEUE_RECV
} kernel_wait_type_t;

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

    kernel_wait_type_t wait_type;
    void *wait_object;
    os_status_t wait_result;

    void *wait_data;
};

#endif