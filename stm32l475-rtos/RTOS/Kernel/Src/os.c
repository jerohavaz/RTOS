#include "os.h"
#include "k_task.h"
#include "k_sched.h"
#include "k_idle.h"
#include "k_timeout.h"
#include "trace.h"

void os_init(void) {
    trace_init();
    k_timeout_init();
    k_task_init();
    k_sched_init();
    k_idle_create();
}

void os_start(void) {
    k_task_lock_creation();
    k_sched_start();
}