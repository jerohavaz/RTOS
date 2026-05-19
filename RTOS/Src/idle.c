#include "idle.h"
#include <stdint.h>
#include "cmsis_gcc.h"
#include "task.h"

void KERNEL_IdleTask() {
    while (1) {
        __WFI();
    }
}

void KERNEL_CreateIdleTask() {
    KERNEL_TaskCreateInternal(KERNEL_IdleTask, RTOS_IDLE_TASK_PRIORITY);
}