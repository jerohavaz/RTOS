#include "scheduler.h"
#include "rtos_config.h"
#include "stm32l475xx.h"
#include "tcb.h"
#include <stdint.h>

extern TCB_sctTCB_t g_asTaskList[RTOS_TOTAL_TASK_COUNT];
volatile TCB_sctTCB_t *g_psCurrentTCB = 0;
uint32_t g_u32CurrentTCBIndex = 0;

extern void Port_StartFirstTaskAsm(void);

void Scheduler_Init() {
    g_psCurrentTCB = 0;
    g_u32CurrentTCBIndex = 0;

    NVIC_SetPriority(PendSV_IRQn, 0xFFu);
    NVIC_SetPriority(SysTick_IRQn, 0xFEu);
    NVIC_SetPriority(SVCall_IRQn, 0xFDu);

    for (uint32_t i = 0; i < RTOS_TOTAL_TASK_COUNT; i++) {
        g_asTaskList[i].eTaskState = TaskState_Ready;
    }
}

uint32_t Scheduler_PeekNextIndex(void) {
    uint32_t start = 0U;

    if (g_psCurrentTCB != 0) {
        start = (g_u32CurrentTCBIndex + 1U) % RTOS_APP_TASK_COUNT;
    }

    for (uint32_t offset = 0U; offset < RTOS_APP_TASK_COUNT; offset++) {
        uint32_t index = (start + offset) % RTOS_APP_TASK_COUNT;

        if (g_asTaskList[index].eTaskState == TaskState_Ready) {
            return index;
        }
    }

    return RTOS_IDLE_TASK_INDEX;
}

void Scheduler_Start() {
    uint32_t u32NextIndex = Scheduler_PeekNextIndex();
    g_psCurrentTCB = &g_asTaskList[u32NextIndex];
    g_u32CurrentTCBIndex = u32NextIndex;

    Port_StartFirstTaskAsm();

    Kernel_Panic();
}

void Scheduler_OnFirstTaskStart() {
    if (g_psCurrentTCB == 0) {
        Kernel_Panic();
    }

    g_psCurrentTCB->eTaskState = TaskState_Running;
}

void Scheduler_SwitchContext() {
    if (g_psCurrentTCB == 0) {
        Kernel_Panic();
    }

    g_psCurrentTCB->eTaskState = TaskState_Ready;

    uint32_t u32Next = Scheduler_PeekNextIndex();

    g_psCurrentTCB = &g_asTaskList[u32Next];
    g_u32CurrentTCBIndex = u32Next;

    if (g_psCurrentTCB == 0) {
        Kernel_Panic();
    }

    g_psCurrentTCB->eTaskState = TaskState_Running;
}

void Scheduler_RequestContextSwitch() {
    uint32_t u32NextPeek = Scheduler_PeekNextIndex();

    if (g_u32CurrentTCBIndex == u32NextPeek) {
        return;
    }

    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk; // System Control Block -> Interupt Control and State Register
    __DSB(); // Data synchronization barrier
    __ISB(); // Instruction synchronization barrier
}

void Kernel_Panic(void) {
    __disable_irq();

    while (1) {
        __asm volatile("bkpt #0");
    }
}