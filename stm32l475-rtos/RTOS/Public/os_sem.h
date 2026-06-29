#ifndef OS_SEM_H_
#define OS_SEM_H_

#include "os_types.h"
#include "prio_waitq.h"
#include "task_list.h"
#include <stdint.h>

typedef struct {
    uint32_t count;
    uint32_t max_count;
    prio_waitq_t wait_list;
} os_sem_t;

os_status_t os_sem_init(os_sem_t *sem, uint32_t initial_count, uint32_t max_count);
os_status_t os_sem_acquire(os_sem_t *sem, uint32_t timeout_ticks);
os_status_t os_sem_release(os_sem_t *sem);

#endif