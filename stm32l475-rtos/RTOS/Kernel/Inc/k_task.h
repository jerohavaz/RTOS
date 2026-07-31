#ifndef K_TASK_H_
#define K_TASK_H_

#include "os_types.h"
#include "os_task.h"
#include "kernel_task.h"
#include <stdint.h>

#define K_MAX_TASKS OS_MAX_TASKS + 1u

void k_task_init(void);
void k_task_lock_creation(void);

os_status_t k_task_create_internal(os_task_func_t task_func,
                                   uint8_t prio,
                                   kernel_task_t **out_task);

kernel_task_t *k_task_get(uint32_t index);
uint32_t k_task_count(void);

#endif