#ifndef OS_TASK_H_
#define OS_TASK_H_

#include "os_types.h"
#include <stdint.h>

typedef void (*os_task_func_t)(void);

os_status_t os_task_create(os_task_func_t task_func, uint8_t prio);

#endif