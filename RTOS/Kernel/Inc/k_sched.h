#ifndef K_SCHED_H_
#define K_SCHED_H_

#include "tcb.h"

void k_sched_init(void);
void k_sched_start(void);
void k_sched_first_task_started(void);
void k_sched_switch(void);
void k_sched_request_switch(void);

void k_sched_make_ready(tcb_t *task);
tcb_t *k_sched_current(void);

#endif