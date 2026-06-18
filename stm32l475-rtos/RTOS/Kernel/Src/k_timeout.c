#include "k_timeout.h"
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
    timeout_list_add(&g_timeout_list, task, k_tick_get() + delay_ticks);
}

void k_timeout_remove(kernel_task_t *task) {
    timeout_list_remove(&g_timeout_list, task);
}

void k_timeout_process_tick(void) {
    kernel_task_t *task;
    uint32_t key;

    key = port_enter_critical();

    while ((task = timeout_list_pop_expired(&g_timeout_list, k_tick_get())) != 0) {
        // TODO: ADD CLEANUP SWITCH CASE

        task->wait_object = 0;
        task->wait_result = OS_ERR_TIMEOUT;

        k_sched_task_ready(task);

        /*
         * No need for sched_request_switch() here. The task will be picked up when systick handler
         * calls k_sched_request_tick_switch() after processing all expired timeouts.
         */
    }

    port_exit_critical(key);
}