#ifndef TASK_H_
#define TASK_H_

#include "rtos_types.h"
#include "tcb.h"
#include <stdint.h>

typedef void (*TaskFunction_t)(void);

RTOS_Status_t RTOS_TaskCreate(TaskFunction_t pfTaskFunc, uint8_t u8TaskPrio);

void KERNEL_TaskSystemInit(void);
void KERNEL_TaskLockCreation(void);

RTOS_Status_t KERNEL_TaskCreateInternal(TaskFunction_t pfTaskFunc, uint8_t u8TaskPrio);

TCB_sctTCB_t *KERNEL_TaskGetByIndex(uint32_t u32Index);
uint32_t KERNEL_TaskGetCount(void);

#endif