#ifndef K_SCHED_H
#define K_SCHED_H

#include "kernel_task.h"
#include <stdint.h>

void k_sched_init(void);
void k_sched_start(void);

void k_sched_set_idle_task(kernel_task_t *task);

uint32_t *k_sched_start_first_context(void);
uint32_t *k_sched_switch_context(uint32_t *outgoing_sp);

void k_sched_task_ready(kernel_task_t *task);
void k_sched_task_block(kernel_task_t *task);

uint8_t k_sched_request_switch(void);
uint8_t k_sched_request_yield(void);

kernel_task_t *k_sched_current(void);

uint8_t k_sched_started(void);

#endif