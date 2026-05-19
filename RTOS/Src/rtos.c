#include "rtos.h"
#include "task.h"
#include "idle.h"
#include "scheduler.h"

void RTOS_Init(void) {
    KERNEL_TaskSystemInit();
    Scheduler_Init();
    KERNEL_CreateIdleTask();
}

void RTOS_Start(void) {
    KERNEL_TaskLockCreation();
    Scheduler_Start();
}