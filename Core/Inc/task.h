#ifndef TASK_H_
#define TASK_H_

#include "tcb.h"
#include <stdint.h>

typedef void (*TaskFunction_t)(void);

void Task_Create(TCB_sctTCB_t *psTask,
                 TaskFunction_t pfTaskFunc,
                 uint8_t u8TaskId,
                 uint8_t u8TaskPrio);

#endif