#include "k_idle.h"
#include "k_task.h"
#include "os_config.h"
#include "port.h"

void k_idle_create(void) {
    k_task_create_internal(k_idle_task, OS_IDLE_TASK_PRIORITY);
}

void k_idle_task(void) {
    while (1) {
        port_wait_for_interrupt();
    }
}