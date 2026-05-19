#include "app.h"
#include "app_tasks.h"
#include "task.h"

void APP_Init(void) {
    RTOS_TaskCreate(App_Task1, 2u);
    RTOS_TaskCreate(App_Task2, 2u);
}