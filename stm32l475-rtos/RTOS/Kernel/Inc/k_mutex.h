#ifndef K_MUTEX_H_
#define K_MUTEX_H_

#include "os_mutex.h"
#include "kernel_task.h"

void k_mutex_timeout_cleanup(os_mutex_t *mutex, kernel_task_t *task);

#endif