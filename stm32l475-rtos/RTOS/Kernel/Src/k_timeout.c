/**
 * @file k_timeout.c
 * @brief Kernel tick and task-timeout implementation.
 * @author Jerome
 */

#include "k_timeout.h"
#include "k_delay.h"
#include "k_mutex.h"
#include "k_queue.h"
#include "k_sem.h"
#include "kernel_panic.h"
#include "k_sched.h"
#include "port.h"
#include "timeout_list.h"
#include "os_types.h"
#include "trace.h"
#include <stdbool.h>

static volatile uint32_t g_tick = 0u;
static timeout_list_t g_timeout_list;

void k_timeout_init(void) {
    g_tick = 0u;
    timeout_list_init(&g_timeout_list);
}

void k_tick_inc(void) {
    g_tick++;
}

uint32_t k_tick_get(void) {
    return g_tick;
}

void k_timeout_add(kernel_task_t *task, uint32_t delay_ticks) {
    KERNEL_REQUIRE(task != 0);
    KERNEL_REQUIRE(delay_ticks != 0u);

    /*
     * timeout_list ordering uses signed tick subtraction, so delays must stay
     * below 2^31 ticks.
     */
    KERNEL_REQUIRE(delay_ticks < K_TIMEOUT_MAX);

    timeout_list_add(&g_timeout_list, task, k_tick_get() + delay_ticks);
}

void k_timeout_remove(kernel_task_t *task) {
    timeout_list_remove(&g_timeout_list, task);
}

bool k_timeout_try_remove(kernel_task_t *task) {
    return timeout_list_try_remove(&g_timeout_list, task);
}

void k_timeout_process_tick(void) {
    kernel_task_t *task;

    uint32_t key = port_enter_critical();
    uint32_t now = k_tick_get();

    while ((task = timeout_list_pop_expired(&g_timeout_list, now)) != 0) {
        KERNEL_REQUIRE(task->tcb.state == TASK_STATE_BLOCKED);

        const void *object = task->wait_object;

        switch (task->wait_type) {
            case K_WAIT_DELAY:
                k_delay_timeout_cleanup(task);
                break;

            case K_WAIT_SEM:
                k_sem_timeout_cleanup((os_sem_t *)object, task);
                break;

            case K_WAIT_MUTEX:
                k_mutex_timeout_cleanup((os_mutex_t *)object, task);
                break;

            case K_WAIT_QUEUE_SEND:
                k_queue_send_timeout_cleanup((os_queue_t *)object, task);
                break;

            case K_WAIT_QUEUE_RECV:
                k_queue_recv_timeout_cleanup((os_queue_t *)object, task);
                break;

            case K_WAIT_NONE:
            default:
                KERNEL_PANIC();
                break;
        }

        KERNEL_REQUIRE(task->wait_type == K_WAIT_NONE);
        KERNEL_REQUIRE(task->wait_object == 0);
        KERNEL_REQUIRE(task->wait_data == 0);

        k_sched_task_ready(task);

        /*
         * No need to request a context switch here. The SysTick handler requests
         * scheduling after all expired timeouts have been processed.
         */
    }

    port_exit_critical(key);
}