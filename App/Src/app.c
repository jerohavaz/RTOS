#include "app.h"
#include "app_tasks.h"
#include "os_task.h"

void app_init(void) {
    os_task_create(app_task1, 2u);
    os_task_create(app_task2, 2u);
}