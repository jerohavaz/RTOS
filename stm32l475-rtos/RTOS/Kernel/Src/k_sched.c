#include "k_sched.h"
#include "k_panic.h"
#include "k_task.h"
#include "kernel_task.h"
#include "os_config.h"
#include "port.h"
#include "prio_waitq.h"
#include "tcb.h"
#include "trace.h"
#include <stdint.h>

static uint8_t g_sched_started = 0u;

static kernel_task_t *g_current_task = 0;
static kernel_task_t *g_idle_task = 0;

static prio_waitq_t g_ready_queue;

static kernel_task_list_node_t *sched_node(kernel_task_t *task) {
    return &task->sched_node;
}

static uint8_t sched_is_idle(const kernel_task_t *task) {
    return (task != 0) && (task == g_idle_task);
}

static void sched_trace_ready(kernel_task_t *task) {
#if OS_TRACE_ENABLED
    if (!sched_is_idle(task)) {
        trace_task_ready(&task->tcb);
    }
#else
    (void)task;
#endif
}

static void sched_trace_blocked(kernel_task_t *task) {
#if OS_TRACE_ENABLED
    if (!sched_is_idle(task)) {
        trace_task_block(&task->tcb);
    }
#else
    (void)task;
#endif
}

static void sched_trace_selected(kernel_task_t *task) {
#if OS_TRACE_ENABLED
    if (sched_is_idle(task)) {
        trace_idle();
    } else {
        trace_task_run(&task->tcb);
    }
#else
    (void)task;
#endif
}

static void sched_task_ready(kernel_task_t *task) {
    if (task == 0) {
        k_panic();
    }

    /*
     * Idle is never queue-managed.
     *
     * The ready queue contains real runnable work only.
     * Idle is selected only when the ready queue is empty.
     */
    if (sched_is_idle(task)) {
        return;
    }

    task->tcb.eTaskState = TaskState_Ready;
    prio_waitq_push(&g_ready_queue, task);

    sched_trace_ready(task);
}

static void sched_task_block(kernel_task_t *task) {
    if (task == 0) {
        k_panic();
    }

    if (sched_is_idle(task)) {
        k_panic();
    }

    task->tcb.eTaskState = TaskState_Blocked;
    sched_trace_blocked(task);
}

static kernel_task_t *sched_pick_next(void) {
    kernel_task_t *next = prio_waitq_pop_highest(&g_ready_queue);

    if (next != 0) {
        return next;
    }

    if (g_idle_task == 0) {
        k_panic();
    }

    return g_idle_task;
}

static kernel_task_t *sched_peek_next(void) {
    kernel_task_t *next = prio_waitq_peek_highest(&g_ready_queue);

    if (next != 0) {
        return next;
    }

    return g_idle_task;
}

static void sched_select_running(kernel_task_t *task) {
    if (task == 0) {
        k_panic();
    }

    task->tcb.eTaskState = TaskState_Running;
    g_current_task = task;

    sched_trace_selected(task);
}

static uint8_t sched_switch_needed(uint8_t allow_same_prio) {
    kernel_task_t *current = g_current_task;
    kernel_task_t *next = sched_peek_next();

    if ((current == 0) || (next == 0)) {
        return 0u;
    }

    /*
     * Current blocked/exited itself.
     * Scheduler must select another context, possibly idle.
     */
    if (current->tcb.eTaskState != TaskState_Running) {
        return 1u;
    }

    /*
     * Idle must yield to any real ready task.
     */
    if (sched_is_idle(current)) {
        if (!sched_is_idle(next)) {
            return 1u;
        }

        return 0u;
    }

    /*
     * A real running task must not be replaced by idle.
     */
    if (sched_is_idle(next)) {
        return 0u;
    }

    /*
     * Defensive: current should not normally be in the ready queue.
     */
    if (current == next) {
        return 0u;
    }

    /*
     * Higher-priority ready task always preempts.
     */
    if (next->tcb.u8TaskPrio > current->tcb.u8TaskPrio) {
        return 1u;
    }

    /*
     * Same-priority ready task only rotates on yield/SysTick.
     */
    if ((allow_same_prio != 0u) && (next->tcb.u8TaskPrio == current->tcb.u8TaskPrio)) {
        return 1u;
    }

    return 0u;
}

static uint8_t sched_request_switch(uint8_t allow_same_prio) {
    if (g_sched_started == 0u) {
        return 0u;
    }

    if (!sched_switch_needed(allow_same_prio)) {
        return 0u;
    }

    port_request_context_switch();

    return 1u;
}

void k_sched_init(void) {
    g_current_task = 0;
    g_idle_task = 0;
    g_sched_started = 0u;

    prio_waitq_init(&g_ready_queue, sched_node);

    port_init_scheduler_interrupts();
}

void k_sched_set_idle_task(kernel_task_t *task) {
    if (task == 0) {
        k_panic();
    }

    if (g_idle_task != 0) {
        k_panic();
    }

    g_idle_task = task;
}

void k_sched_start(void) {
    if (g_idle_task == 0) {
        k_panic();
    }

    k_task_lock_creation();

    uint32_t task_count = k_task_count();

    for (uint32_t i = 0u; i < task_count; i++) {
        kernel_task_t *task = k_task_get(i);

        if (task == 0) {
            k_panic();
        }

        if (sched_is_idle(task)) {
            continue;
        }

        if (task->tcb.eTaskState == TaskState_Created) {
            sched_task_ready(task);
        }
    }

    g_current_task = sched_pick_next();

    port_start_first_task();

    k_panic();
}

port_stack_t *k_sched_start_first_context(void) {
    if (g_current_task == 0) {
        k_panic();
    }

    if (g_current_task->tcb.pu32TaskSP == 0) {
        k_panic();
    }

    g_current_task->tcb.eTaskState = TaskState_Running;

    sched_trace_selected(g_current_task);

    g_sched_started = 1u;

    return g_current_task->tcb.pu32TaskSP;
}

void k_sched_task_ready(kernel_task_t *task) {
    uint32_t key = port_enter_critical();

    sched_task_ready(task);

    port_exit_critical(key);
}

void k_sched_task_block(kernel_task_t *task) {
    uint32_t key = port_enter_critical();

    sched_task_block(task);

    port_exit_critical(key);
}

/*
 * Kernel wakeup/preemption request.
 *
 * Use after a task was made Ready by timeout, message queue, semaphore,
 * event flag, ISR wakeup, etc.
 *
 * Same-priority tasks do not preempt here. They rotate on yield/SysTick.
 */
uint8_t k_sched_request_switch(void) {
    uint32_t key = port_enter_critical();
    uint8_t requested = sched_request_switch(0u);

    port_exit_critical(key);

    return requested;
}

/*
 * Yield scheduling request.
 *
 * Allows same-priority round-robin in addition to normal preemption.
 */
uint8_t k_sched_request_yield(void) {
    uint32_t key = port_enter_critical();
    uint8_t requested = sched_request_switch(1u);

    port_exit_critical(key);

    return requested;
}

port_stack_t *k_sched_switch_context(port_stack_t *outgoing_sp) {
    if (outgoing_sp == 0) {
        k_panic();
    }

    uint32_t key = port_enter_critical();

    kernel_task_t *old = g_current_task;

    if (old == 0) {
        port_exit_critical(key);
        k_panic();
    }

    /*
     * PendSV already saved R4-R11, so this SP is the complete software-saved
     * task context.
     */
    old->tcb.pu32TaskSP = outgoing_sp;

    /*
     * If old is still Running, it was preempted or time-sliced and becomes
     * Ready again. If it blocked before PendSV, its state is already Blocked
     * and it must not be reinserted.
     *
     * Idle is never queue-managed.
     */
    if ((old->tcb.eTaskState == TaskState_Running) && !sched_is_idle(old)) {
        sched_task_ready(old);
    }

    kernel_task_t *next = sched_pick_next();
    sched_select_running(next);

    uint32_t *incoming_sp = next->tcb.pu32TaskSP;

    if (incoming_sp == 0) {
        port_exit_critical(key);
        k_panic();
    }

    port_exit_critical(key);

    return incoming_sp;
}

kernel_task_t *k_sched_current(void) {
    uint32_t key = port_enter_critical();
    kernel_task_t *task = g_current_task;

    port_exit_critical(key);

    return task;
}

uint8_t k_sched_started(void) {
    return g_sched_started;
}