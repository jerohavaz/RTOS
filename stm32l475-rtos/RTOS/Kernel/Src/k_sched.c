#include "k_sched.h"
#include "kernel_panic.h"
#include "k_task.h"
#include "kernel_task.h"
#include "os_config.h"
#include "port.h"
#include "prio_waitq.h"
#include "tcb.h"
#include "trace.h"
#include <stdint.h>
#include <stdbool.h>

static bool g_sched_started = false;

static kernel_task_t *g_current_task = 0;
static kernel_task_t *g_idle_task = 0;

static prio_waitq_t g_ready_queue;

static kernel_task_list_node_t *sched_node(kernel_task_t *task) {
    return &task->sched_node;
}

static void sched_task_set_state(kernel_task_t *task, TCB_eTaskStates_t state) {
    KERNEL_REQUIRE(task != 0);

    TCB_eTaskStates_t old_state = task->tcb.eTaskState;

    trace_task_state(task->tcb.u8TaskId, (uint8_t)old_state, (uint8_t)state);

    switch (state) {
        case TaskState_Ready:
            task->tcb.eTaskState = TaskState_Ready;

            /*
             * Idle is never queue-managed.
             *
             * The ready queue contains real runnable work only.
             * Idle is selected only when the ready queue is empty.
             */
            if (k_sched_is_idle(task)) {
                return;
            }

            prio_waitq_push(&g_ready_queue, task);
            trace_task_ready(&task->tcb);
            break;

        case TaskState_Blocked:
            KERNEL_REQUIRE(!k_sched_is_idle(task));

            task->tcb.eTaskState = TaskState_Blocked;
            trace_task_block(&task->tcb);
            break;

        case TaskState_Running:
            task->tcb.eTaskState = TaskState_Running;
            g_current_task = task;

            if (k_sched_is_idle(task)) {
                trace_idle();
            } else {
                trace_task_run(&task->tcb);
            }
            break;

        default:
            KERNEL_PANIC();
            break;
    }
}

static kernel_task_t *sched_pick_next(void) {
    kernel_task_t *next = prio_waitq_pop_highest(&g_ready_queue);

    if (next != 0) {
        return next;
    }

    KERNEL_REQUIRE(g_idle_task != 0);

    return g_idle_task;
}

static kernel_task_t *sched_peek_next(void) {
    kernel_task_t *next = prio_waitq_peek_highest(&g_ready_queue);

    if (next != 0) {
        return next;
    }

    return g_idle_task;
}

static bool sched_switch_needed(bool allow_same_prio) {
    const kernel_task_t *current = g_current_task;
    KERNEL_REQUIRE(current != 0);
    KERNEL_REQUIRE(g_idle_task != 0);

    /*
     * Current blocked/exited itself.
     * Scheduler must select another context, possibly idle.
     */
    if (current->tcb.eTaskState != TaskState_Running) {
        return true;
    }

    const kernel_task_t *next = sched_peek_next();
    KERNEL_REQUIRE(next != 0);

    /*
     * Idle must yield to any real ready task.
     * If next is also idle, no real work exists.
     */
    if (k_sched_is_idle(current)) {
        return !k_sched_is_idle(next);
    }

    /*
     * A real running task must not be replaced by idle.
     */
    if (k_sched_is_idle(next)) {
        return false;
    }

    /*
     * From here, both current and next are real tasks.
     * Current should not normally be in the ready queue.
     */
    KERNEL_REQUIRE(current != next);

    /*
     * Higher-priority ready task always preempts.
     * This assumes larger u8TaskPrio means higher priority.
     */
    if (next->tcb.u8TaskPrio > current->tcb.u8TaskPrio) {
        return true;
    }

    /*
     * Same-priority ready task only rotates on yield/SysTick.
     */
    if (allow_same_prio && (next->tcb.u8TaskPrio == current->tcb.u8TaskPrio)) {
        return true;
    }

    return false;
}

static bool sched_request_switch(bool allow_same_prio) {
    if (!g_sched_started) {
        return false;
    }

    if (!sched_switch_needed(allow_same_prio)) {
        return false;
    }

    port_request_context_switch();

    return true;
}

void k_sched_init(void) {
    g_current_task = 0;
    g_idle_task = 0;
    g_sched_started = false;

    prio_waitq_init(&g_ready_queue, sched_node);

    port_init_scheduler_interrupts();
}

void k_sched_set_idle_task(kernel_task_t *task) {
    KERNEL_REQUIRE(task != 0);
    KERNEL_REQUIRE(g_idle_task == 0);

    g_idle_task = task;
}

bool k_sched_is_idle(const kernel_task_t *task) {
    return (task != 0) && (task == g_idle_task);
}

void k_sched_start(void) {
    KERNEL_REQUIRE(g_idle_task != 0);

    uint32_t task_count = k_task_count();

    for (uint32_t i = 0u; i < task_count; i++) {
        kernel_task_t *task = k_task_get(i);

        KERNEL_REQUIRE(task != 0);

        if (task->tcb.eTaskState == TaskState_Created) {
            sched_task_set_state(task, TaskState_Ready);
        }
    }

    g_current_task = sched_pick_next();

    port_start_first_task();

    KERNEL_PANIC();
}

port_stack_t *k_sched_start_first_context(void) {
    KERNEL_REQUIRE(g_current_task != 0);
    KERNEL_REQUIRE(g_current_task->tcb.pu32TaskSP != 0);

    sched_task_set_state(g_current_task, TaskState_Running);

    g_sched_started = true;

    return g_current_task->tcb.pu32TaskSP;
}

void k_sched_task_ready(kernel_task_t *task) {
    uint32_t key = port_enter_critical();

    sched_task_set_state(task, TaskState_Ready);

    port_exit_critical(key);
}

void k_sched_task_block(kernel_task_t *task) {
    uint32_t key = port_enter_critical();

    sched_task_set_state(task, TaskState_Blocked);

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
bool k_sched_request_switch(void) {
    uint32_t key = port_enter_critical();
    bool requested = sched_request_switch(false);

    port_exit_critical(key);

    return requested;
}

/*
 * Yield scheduling request.
 *
 * Allows same-priority round-robin in addition to normal preemption.
 */
bool k_sched_request_yield(void) {
    uint32_t key = port_enter_critical();
    bool requested = sched_request_switch(true);

    port_exit_critical(key);

    return requested;
}

port_stack_t *k_sched_switch_context(port_stack_t *outgoing_sp) {
    KERNEL_REQUIRE(outgoing_sp != 0);

    uint32_t key = port_enter_critical();

    kernel_task_t *old = g_current_task;

    if (old == 0) {
        port_exit_critical(key);
        KERNEL_PANIC();
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
     */
    if (old->tcb.eTaskState == TaskState_Running) {
        sched_task_set_state(old, TaskState_Ready);
    }

    kernel_task_t *next = sched_pick_next();

    KERNEL_REQUIRE(next != 0);

    sched_task_set_state(next, TaskState_Running);

    port_stack_t *incoming_sp = next->tcb.pu32TaskSP;

    if (incoming_sp == 0) {
        port_exit_critical(key);
        KERNEL_PANIC();
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

bool k_sched_started(void) {
    return g_sched_started;
}