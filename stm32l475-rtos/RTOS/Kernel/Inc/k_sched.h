#ifndef K_SCHED_H_
#define K_SCHED_H_

#include "tcb.h"
#include <stdint.h>

void k_sched_init(void);
void k_sched_start(void);
void k_sched_first_task_started(void);

void k_sched_task_ready(tcb_t *task);
void k_sched_task_block(tcb_t *task);
void k_sched_task_unblock(tcb_t *task);

void k_sched_switch(void);

uint8_t k_sched_request_switch(void);
tcb_t *k_sched_current(void);

#endif