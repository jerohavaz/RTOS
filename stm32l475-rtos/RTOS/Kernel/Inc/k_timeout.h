#ifndef K_TIMEOUT_H_
#define K_TIMEOUT_H_

#include "kernel_task.h"
#include <stdbool.h>
#include <stdint.h>

#define K_TIMEOUT_MAX 0x80000000u

void k_timeout_init(void);

void k_tick_inc(void);
uint32_t k_tick_get(void);

void k_timeout_add(kernel_task_t *task, uint32_t delay_ticks);
void k_timeout_remove(kernel_task_t *task);
bool k_timeout_try_remove(kernel_task_t *task);
void k_timeout_process_tick(void);

#endif