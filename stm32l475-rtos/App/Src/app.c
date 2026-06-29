#include "app.h"
#include "app_tasks.h"
#include "os_task.h"
#include "os_sem.h"

void app_init(void) {
    mutex_test_init();
}