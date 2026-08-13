#ifndef APP_TASKS_H_
#define APP_TASKS_H_

#include "os_types.h"

typedef enum {
    APP_SENSOR_CMD_MODE_LOW = 0,
    APP_SENSOR_CMD_MODE_NORMAL,
    APP_SENSOR_CMD_MODE_HIGH,
    APP_SENSOR_CMD_RESET,
    APP_SENSOR_CMD_STATUS
} app_sensor_command_t;

os_status_t app_sensor_command_submit(app_sensor_command_t command);

#endif