/**
 * @file integration_sensor_device.h
 * @brief LSM6DSL device access for the sensor application.
 * @author Jerome
 * @author Martin
 */

#ifndef INTEGRATION_SENSOR_DEVICE_H_
#define INTEGRATION_SENSOR_DEVICE_H_

#include <stdbool.h>

#include "integration_sensor_types.h"

/**
 * @brief Initialize the accelerometer, gyroscope, default mode, and interrupt.
 * @return @c true when every device operation succeeds.
 */
bool sensor_device_init(void);

/**
 * @brief Apply an accelerometer and gyroscope sampling mode.
 * @param mode Requested sampling mode.
 * @return @c true when both devices and their registers were updated.
 */
bool sensor_device_set_mode(sensor_mode_t mode);

/**
 * @brief Read and convert one six-axis sensor sample.
 * @param[out] sample Destination for acceleration in g and angular rate in dps.
 * @return @c true on success; @c false for a null destination or I2C failure.
 */
bool sensor_device_read(sensor_sample_t *sample);

/**
 * @brief Read the active accelerometer and gyroscope control registers.
 * @param[out] status Destination for the register values.
 * @return @c true when both registers were read successfully.
 */
bool sensor_device_read_status(sensor_device_status_t *status);

/**
 * @brief Software-reset and reinitialize the LSM6DSL.
 * @return @c true when reset completes before the timeout and initialization succeeds.
 */
bool sensor_device_reset(void);

#endif /* INTEGRATION_SENSOR_DEVICE_H_ */
