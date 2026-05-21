#include "k_sched.h"
#include "k_task.h"
#include "k_panic.h"
#include "port.h"
#include "prio_waitq.h"
#include "stm32l475xx.h"
#include "tcb.h"

static uint8_t g_sched_started = 0u;
volatile tcb_t *g_current_tcb = 0;

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

void k_sched_make_ready(tcb_t *task) {
    task->state = TASK_STATE_READY;
    prio_waitq_push(&g_ready_queue, task);
}

void k_sched_start(void) {
    uint32_t task_count = k_task_count();

    for (uint32_t i = 0u; i < task_count; i++) {
        tcb_t *task = k_task_get(i);

        if ((task != 0) && (task->state == TASK_STATE_CREATED)) {
            k_sched_make_ready(task);
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
    g_sched_started = 1u;
}

void k_sched_switch(void) {
    uint32_t irq = port_enter_critical();
    if (g_current_tcb == 0) {
        k_panic();
    }

    if (g_current_tcb->state == TASK_STATE_RUNNING) {
        k_sched_make_ready((tcb_t *)g_current_tcb);
    }

    g_current_tcb = prio_waitq_pop_highest(&g_ready_queue);

    if (g_current_tcb == 0) {
        k_panic();
    }

    g_current_tcb->state = TASK_STATE_RUNNING;
    port_exit_critical(irq);
}

void k_sched_request_switch(void) {
    if (g_sched_started == 0u) {
        return;
    }

    tcb_t *candidate = prio_waitq_peek_highest(&g_ready_queue);

    if (candidate != 0 && candidate->prio >= g_current_tcb->prio) {
        port_request_context_switch();
    }
}

tcb_t *k_sched_current(void) {
    return (tcb_t *)g_current_tcb;
}