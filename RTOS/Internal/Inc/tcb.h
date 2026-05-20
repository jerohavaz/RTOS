#ifndef TCB_H_
#define TCB_H_

#include "os_config.h"
#include <stdint.h>

typedef enum {
    TASK_STATE_CREATED = 0u,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_DELETED,
} task_state_t;

typedef struct tcb tcb_t;

typedef struct {
    tcb_t *next;
    tcb_t *prev;
} tcb_list_node_t;

struct tcb {
    uint32_t *sp; /* MUST stay first: assembly assumes offset 0 */

    uint8_t id;
    uint8_t prio;
    task_state_t state;

    tcb_list_node_t sched_node;

    uint32_t stack[OS_TASK_STACK_SIZE];
};

#endif