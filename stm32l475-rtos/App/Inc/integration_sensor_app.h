/**
 * @file integration_sensor_app.h
 * @brief Sensor application command interface.
 * @author Jerome
 * @author Martin
 *
 * Defines the commands accepted by the sensor task and the non-blocking API
 * used by the UART shell to submit them.
 */

#ifndef INTEGRATION_SENSOR_APP_H_
#define INTEGRATION_SENSOR_APP_H_

#include "os_types.h"

/** @brief Commands executed asynchronously by the sensor task. */
typedef enum {
    APP_SENSOR_CMD_MODE_LOW = 0, /**< Select 52 Hz low-power sampling. */
    APP_SENSOR_CMD_MODE_NORMAL,  /**< Select 104 Hz normal sampling. */
    APP_SENSOR_CMD_MODE_HIGH,    /**< Select 416 Hz high-rate sampling. */
    APP_SENSOR_CMD_RESET,        /**< Reset and reinitialize the LSM6DSL. */
    APP_SENSOR_CMD_STATUS        /**< Read registers and runtime counters. */
} app_sensor_command_t;

/**
 * @brief Queue a command for the sensor task.
 *
 * The command is copied into the command queue without blocking. The sensor
 * semaphore is then released so a command is handled even when no data-ready
 * interrupt is pending.
 *
 * @param command Command to execute.
 * @return @c OS_OK when queued, otherwise the queue or semaphore error.
 */
os_status_t app_sensor_command_submit(app_sensor_command_t command);

#endif /* INTEGRATION_SENSOR_APP_H_ */
