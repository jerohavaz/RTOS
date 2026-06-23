#ifndef K_DELAY_H_
#define K_DELAY_H_

#include "kernel_task.h"
#include "os_sem.h"
#include "task_list.h"

void k_delay_timeout_cleanup(kernel_task_t *task);

#endif