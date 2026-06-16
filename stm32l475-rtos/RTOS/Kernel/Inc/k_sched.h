#ifndef K_SCHED_H_
#define K_SCHED_H_

#include "kernel_task.h"
#include "tcb.h"

#include <stdint.h>

void k_sched_init(void);
void k_sched_start(void);
void k_sched_first_task_started(void);

void k_sched_task_ready(kernel_task_t *task);
void k_sched_task_block(kernel_task_t *task);
void k_sched_task_unblock(kernel_task_t *task);

void k_sched_switch(void);
uint8_t k_sched_request_switch(void);

kernel_task_t *k_sched_current(void);
TCB_sctTCB_t *k_sched_current_tcb(void);

#endif