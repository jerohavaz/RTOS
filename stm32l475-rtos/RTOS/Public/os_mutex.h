#ifndef OS_MUTEX_H_
#define OS_MUTEX_H_

#include "kernel_task.h"
#include "os_types.h"
#include "prio_waitq.h"
#include <stdint.h>

typedef struct {
    kernel_task_t *owner;
    prio_waitq_t wait_list;
} os_mutex_t;

os_status_t os_mutex_init(os_mutex_t *mutex);
os_status_t os_mutex_lock(os_mutex_t *mutex, uint32_t timeout_ticks);
os_status_t os_mutex_unlock(os_mutex_t *mutex);

#endif