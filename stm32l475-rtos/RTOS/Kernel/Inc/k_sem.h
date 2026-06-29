#ifndef K_SEM_H_
#define K_SEM_H_

#include "kernel_task.h"
#include "os_sem.h"
#include "task_list.h"

void k_sem_timeout_cleanup(os_sem_t *sem, kernel_task_t *task);

#endif