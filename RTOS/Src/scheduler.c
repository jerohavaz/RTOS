#include "scheduler.h"
#include "task.h"
#include "rtos_config.h"
#include "stm32l475xx.h"
#include "tcb.h"
#include <stdint.h>

volatile TCB_sctTCB_t *g_psCurrentTCB = 0;
static uint32_t g_u32CurrentTCBIndex = 0;

extern void Port_StartFirstTaskAsm(void);

void Scheduler_Init() {
    g_psCurrentTCB = 0;
    g_u32CurrentTCBIndex = 0;

    NVIC_SetPriority(PendSV_IRQn, 15u);
    NVIC_SetPriority(SysTick_IRQn, 14u);
    NVIC_SetPriority(SVCall_IRQn, 13u);
}

static uint32_t Scheduler_SelectNextIndex(void) {
    uint32_t task_count = KERNEL_TaskGetCount();

    if (task_count == 0u) {
        Kernel_Panic();
    }

    uint32_t best_index = UINT32_MAX;
    uint8_t best_prio = 0u;

    for (uint32_t i = 0u; i < task_count; i++) {
        TCB_sctTCB_t *task = KERNEL_TaskGetByIndex(i);

        if ((task != 0) && (task->eTaskState == TaskState_Ready)) {
            if ((best_index == UINT32_MAX) || (task->u8TaskPrio > best_prio)) {
                best_index = i;
                best_prio = task->u8TaskPrio;
            }
        }
    }

    if (best_index == UINT32_MAX) {
        Kernel_Panic();
    }

    uint32_t start = 0u;

    if (g_psCurrentTCB != 0) {
        start = (g_u32CurrentTCBIndex + 1u) % task_count;
    }

    for (uint32_t offset = 0u; offset < task_count; offset++) {
        uint32_t index = (start + offset) % task_count;
        TCB_sctTCB_t *task = KERNEL_TaskGetByIndex(index);

        if ((task != 0) && (task->eTaskState == TaskState_Ready) &&
            (task->u8TaskPrio == best_prio)) {
            return index;
        }
    }

    Kernel_Panic();
    return 0u;
}

void Scheduler_Start(void) {
    uint32_t task_count = KERNEL_TaskGetCount();

    for (uint32_t i = 0u; i < task_count; i++) {
        TCB_sctTCB_t *task = KERNEL_TaskGetByIndex(i);

        if ((task != 0) && (task->eTaskState == TaskState_Created)) {
            task->eTaskState = TaskState_Ready;
        }
    }

    g_u32CurrentTCBIndex = Scheduler_SelectNextIndex();
    g_psCurrentTCB = KERNEL_TaskGetByIndex(g_u32CurrentTCBIndex);

    if (g_psCurrentTCB == 0) {
        Kernel_Panic();
    }

    Port_StartFirstTaskAsm();

    Kernel_Panic();
}

void Scheduler_OnFirstTaskStart() {
    if (g_psCurrentTCB == 0) {
        Kernel_Panic();
    }

    g_psCurrentTCB->eTaskState = TaskState_Running;
}

void Scheduler_SwitchContext(void) {
    if (g_psCurrentTCB == 0) {
        Kernel_Panic();
    }

    if (g_psCurrentTCB->eTaskState == TaskState_Running) {
        g_psCurrentTCB->eTaskState = TaskState_Ready;
    }

    g_u32CurrentTCBIndex = Scheduler_SelectNextIndex();
    g_psCurrentTCB = KERNEL_TaskGetByIndex(g_u32CurrentTCBIndex);

    if (g_psCurrentTCB == 0) {
        Kernel_Panic();
    }

    g_psCurrentTCB->eTaskState = TaskState_Running;
}

void Scheduler_RequestContextSwitch() {
    uint32_t u32NextPeek = Scheduler_SelectNextIndex();

    if (g_u32CurrentTCBIndex == u32NextPeek) {
        return;
    }

    SCB->ICSR =
        SCB_ICSR_PENDSVSET_Msk; // System Control Block -> Interupt Control and State Register
    __DSB();                    // Data synchronization barrier
    __ISB();                    // Instruction synchronization barrier
}

void Kernel_Panic(void) {
    __disable_irq();

    while (1) {
        __asm volatile("bkpt #0");
    }
}