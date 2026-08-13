/**
 * @file integration_sensor_types.h
 * @brief Data types shared by the sensor application modules.
 * @author Jerome
 * @author Martin
 */

#ifndef INTEGRATION_SENSOR_TYPES_H_
#define INTEGRATION_SENSOR_TYPES_H_

#include <stdint.h>

/** @brief One converted LSM6DSL accelerometer and gyroscope reading. */
typedef struct {
    float acceleration_g[3];   /**< X, Y, and Z acceleration in g. */
    float angular_rate_dps[3]; /**< X, Y, and Z angular rate in degrees per second. */
} sensor_sample_t;

/** @brief Accumulator for one fixed-period output batch. */
typedef struct {
    sensor_sample_t sum; /**< Component-wise sum of all collected readings. */
    uint32_t count;      /**< Number of readings represented by @ref sum. */
} sensor_sample_batch_t;

/** @brief Supported LSM6DSL sampling configurations. */
typedef enum {
    SENSOR_MODE_LOW = 0, /**< 52 Hz with low-power mode enabled. */
    SENSOR_MODE_NORMAL,  /**< 104 Hz with low-power mode disabled. */
    SENSOR_MODE_HIGH     /**< 416 Hz with low-power mode disabled. */
} sensor_mode_t;

/** @brief Device registers reported by the @c status shell command. */
typedef struct {
    uint8_t ctrl1_xl; /**< LSM6DSL @c CTRL1_XL register. */
    uint8_t ctrl2_g;  /**< LSM6DSL @c CTRL2_G register. */
} sensor_device_status_t;

#endif /* INTEGRATION_SENSOR_TYPES_H_ */
