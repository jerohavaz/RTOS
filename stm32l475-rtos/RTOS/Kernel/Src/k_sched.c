#include "k_sched.h"
#include "k_task.h"
#include "k_panic.h"
#include "kernel_task.h"
#include "os_config.h"
#include "port.h"
#include "prio_waitq.h"
#include "stm32l475xx.h"
#include "tcb.h"
#include "trace.h"
#include <stdint.h>

static uint8_t g_sched_started = 0u;
static kernel_task_t *g_current_task = 0;
TCB_sctTCB_t *g_current_tcb = 0;

static prio_waitq_t g_ready_queue;

static kernel_task_list_node_t *sched_node(kernel_task_t *task) {
    return &task->sched_node;
}

static void sched_set_current(kernel_task_t *task) {
    g_current_task = task;
    g_current_tcb = (task != 0) ? &task->tcb : 0;
}

void k_sched_init(void) {
    sched_set_current(0);
    g_sched_started = 0u;

    prio_waitq_init(&g_ready_queue, sched_node);

    NVIC_SetPriority(PendSV_IRQn, 15u);
    NVIC_SetPriority(SysTick_IRQn, 14u);
    NVIC_SetPriority(SVCall_IRQn, 13u);
}

void k_sched_start(void) {
    uint32_t task_count = k_task_count();

    for (uint32_t i = 0u; i < task_count; i++) {
        kernel_task_t *task = k_task_get(i);

        if ((task != 0) && (task->tcb.eTaskState == TaskState_Created)) {
            k_sched_task_ready(task);
        }
    }

    kernel_task_t *first = prio_waitq_pop_highest(&g_ready_queue);

    if (first == 0) {
        k_panic();
    }

    sched_set_current(first);

    port_start_first_task();
    k_panic();
}

void k_sched_first_task_started(void) {
    if ((g_current_task == 0) || (g_current_tcb == 0)) {
        k_panic();
    }

    g_current_tcb->eTaskState = TaskState_Running;
    trace_task_run(g_current_tcb);

    g_sched_started = 1u;
}

void k_sched_task_ready(kernel_task_t *task) {
    if (task == 0) {
        return;
    }

    task->tcb.eTaskState = TaskState_Ready;
    prio_waitq_push(&g_ready_queue, task);
    trace_task_ready(&task->tcb);
}

void k_sched_task_block(kernel_task_t *task) {
    if (task == 0) {
        return;
    }

    task->tcb.eTaskState = TaskState_Blocked;
    trace_task_block(&task->tcb);
}

void k_sched_task_unblock(kernel_task_t *task) {
    if (task == 0) {
        return;
    }

    if (task->tcb.eTaskState != TaskState_Blocked) {
        return;
    }

    k_sched_task_ready(task);
    k_sched_request_switch();
}

void k_sched_switch(void) {
    uint32_t irq = port_enter_critical();

    kernel_task_t *old = g_current_task;

    if (old == 0) {
        k_panic();
    }

    if (old->tcb.eTaskState == TaskState_Running) {
        k_sched_task_ready(old);
    }

    kernel_task_t *next = prio_waitq_pop_highest(&g_ready_queue);

    if (next == 0) {
        k_panic();
    }

    next->tcb.eTaskState = TaskState_Running;
    sched_set_current(next);

    trace_task_run(&next->tcb);

    port_exit_critical(irq);
}

uint8_t k_sched_request_switch(void) {
    if (g_sched_started == 0u) {
        return 0u;
    }

    kernel_task_t *candidate = prio_waitq_peek_highest(&g_ready_queue);

    if (candidate == 0) {
        return 0u;
    }

    port_request_context_switch();
    return 1u;
}

kernel_task_t *k_sched_current(void) {
    return g_current_task;
}

TCB_sctTCB_t *k_sched_current_tcb(void) {
    return g_current_tcb;
}