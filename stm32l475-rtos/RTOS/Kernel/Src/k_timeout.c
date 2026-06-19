#include "k_timeout.h"
#include "kernel_panic.h"
#include "k_sched.h"
#include "port.h"
#include "timeout_list.h"
#include "os_types.h"

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
    KERNEL_REQUIRE(delay_ticks < 0x80000000u);

    timeout_list_add(&g_timeout_list, task, k_tick_get() + delay_ticks);
}

void k_timeout_remove(kernel_task_t *task) {
    timeout_list_remove(&g_timeout_list, task);
}

uint8_t k_timeout_try_remove(kernel_task_t *task) {
    return timeout_list_try_remove(&g_timeout_list, task);
}

void k_timeout_process_tick(void) {
    kernel_task_t *task;

    uint32_t key = port_enter_critical();

    while ((task = timeout_list_pop_expired(&g_timeout_list, k_tick_get())) != 0) {
        KERNEL_REQUIRE(task->tcb.eTaskState == TaskState_Blocked);

        const void *object = task->wait_object;

        switch (task->wait_type) {
            case K_WAIT_DELAY:
                KERNEL_REQUIRE(object == 0);
                task->wait_result = OS_OK;
                break;

            case K_WAIT_SEM:
                KERNEL_REQUIRE(object != 0);
                /*
                 * k_sem_timeout_cleanup((os_sem_t *)object, task);
                 */
                task->wait_result = OS_ERR_TIMEOUT;
                break;

            case K_WAIT_MUTEX:
                KERNEL_REQUIRE(object != 0);
                /*
                 * k_mutex_timeout_cleanup((k_mutex_t *)object, task);
                 */
                task->wait_result = OS_ERR_TIMEOUT;
                break;

            case K_WAIT_QUEUE_SEND:
                KERNEL_REQUIRE(object != 0);
                /*
                 * k_queue_send_timeout_cleanup((k_queue_t *)object, task);
                 */
                task->wait_result = OS_ERR_TIMEOUT;
                break;

            case K_WAIT_QUEUE_RECV:
                KERNEL_REQUIRE(object != 0);
                /*
                 * k_queue_recv_timeout_cleanup((k_queue_t *)object, task);
                 */
                task->wait_result = OS_ERR_TIMEOUT;
                break;

            case K_WAIT_NONE:
            default:
                KERNEL_PANIC();
                break;
        }

        task->wait_object = 0;
        task->wait_type = K_WAIT_NONE;

        k_sched_task_ready(task);

        /*
         * No need to request a context switch here. The SysTick handler requests
         * scheduling after all expired timeouts have been processed.
         */
    }

    port_exit_critical(key);
}