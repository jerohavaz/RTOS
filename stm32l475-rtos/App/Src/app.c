#include "app.h"
#include "app_tasks.h"
#include "os_task.h"
#include "os_sem.h"
#include "os_queue.h"

void app_init(void) {
    app_tasks_init();
}