#include "k_sched.h"
#include "k_task.h"
#include "k_panic.h"
#include "os_config.h"
#include "port.h"
#include "prio_waitq.h"
#include "stm32l475xx.h"
#include "tcb.h"
#include "trace.h"
#include <stdint.h>

static uint8_t g_sched_started = 0u;
tcb_t *g_current_tcb = 0;

static prio_waitq_t g_ready_queue;

static tcb_list_node_t *sched_node(tcb_t *task) {
    return &task->sched_node;
}

void k_sched_init(void) {
    g_current_tcb = 0;
    g_sched_started = 0u;

    prio_waitq_init(&g_ready_queue, sched_node);

    NVIC_SetPriority(PendSV_IRQn, 15u);
    NVIC_SetPriority(SysTick_IRQn, 14u);
    NVIC_SetPriority(SVCall_IRQn, 13u);
}

void k_sched_start(void) {
    uint32_t task_count = k_task_count();

    for (uint32_t i = 0u; i < task_count; i++) {
        tcb_t *task = k_task_get(i);

        if ((task != 0) && (task->state == TASK_STATE_CREATED)) {
            k_sched_task_ready(task);
        }
    }

    g_current_tcb = prio_waitq_pop_highest(&g_ready_queue);
    if (g_current_tcb == 0) {
        k_panic();
    }

    port_start_first_task();
    k_panic();
}

void k_sched_first_task_started(void) {
    if (g_current_tcb == 0) {
        k_panic();
    }

    g_current_tcb->state = TASK_STATE_RUNNING;
    trace_task_run(g_current_tcb);

    g_sched_started = 1u;
}

void k_sched_task_ready(tcb_t *task) {
    task->state = TASK_STATE_READY;
    prio_waitq_push(&g_ready_queue, task);
    trace_task_ready(task);
}

void k_sched_task_block(tcb_t *task) {
    task->state = TASK_STATE_BLOCKED;
    prio_waitq_remove(&g_ready_queue, task);
    trace_task_block(task);
    k_sched_request_switch();
}

void k_sched_task_unblock(tcb_t *task) {
    if (task->state != TASK_STATE_BLOCKED) {
        return;
    }

    k_sched_task_ready(task);
    k_sched_request_switch();
}

void k_sched_switch(void) {
    uint32_t irq = port_enter_critical();

    tcb_t *old = g_current_tcb;
    if (old == 0) {
        k_panic();
    }

    if (old->state == TASK_STATE_RUNNING) {
        k_sched_task_ready(old);
    }

    tcb_t *next = prio_waitq_pop_highest(&g_ready_queue);
    if (next == 0) {
        k_panic();
    }

    next->state = TASK_STATE_RUNNING;
    g_current_tcb = next;
    trace_task_run(next);
    
    port_exit_critical(irq);
}

uint8_t k_sched_request_switch(void) {
    if (g_sched_started == 0u) {
        return 0;
    }

    tcb_t *candidate = prio_waitq_peek_highest(&g_ready_queue);
    if (candidate == 0) {
        return 0;
    }

    port_request_context_switch();
    return 1;
}

tcb_t *k_sched_current(void) {
    return g_current_tcb;
}