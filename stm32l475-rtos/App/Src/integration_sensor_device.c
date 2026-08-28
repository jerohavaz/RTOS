/**
 * @file integration_sensor_device.c
 * @brief LSM6DSL initialization, register access, and sample conversion.
 * @author Jerome
 * @author Martin
 *
 * Uses the board BSP for device setup and low-power selection, and direct I2C
 * register access for synchronized six-axis reads, status, and reset control.
 */

#include "integration_sensor_device.h"

#include "project.h"

#if PROJECT == PROJECT_SENSOR

#include <stdint.h>

#include "main.h"
#include "os_delay.h"
#include "stm32l475e_iot01_accelero.h"
#include "stm32l475e_iot01_gyro.h"

#define LSM6DSL_INT1_DRDY_XL_BIT 0x01u /**< Route accelerometer data-ready to INT1. */
#define LSM6DSL_DRDY_PULSED_BIT  0x80u /**< Select pulsed data-ready signaling. */
#define LSM6DSL_SW_RESET_BIT     0x01u /**< Software-reset bit in @c CTRL3_C. */
#define SENSOR_IO_TIMEOUT_MS     100u  /**< HAL I2C transaction timeout. */
#define SENSOR_RESET_TIMEOUT_MS  20u   /**< Maximum software-reset wait. */

/** @brief I2C peripheral connected to the board sensor. */
extern I2C_HandleTypeDef hi2c2;

/**
 * @brief Read one LSM6DSL register.
 * @param reg Register address.
 * @param[out] value Destination byte.
 * @return @c true when the HAL transaction succeeds.
 */
static bool sensor_reg_read(uint8_t reg, uint8_t *value) {
    return HAL_I2C_Mem_Read(&hi2c2,
                            LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW,
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            value,
                            1u,
                            SENSOR_IO_TIMEOUT_MS) == HAL_OK;
}

/**
 * @brief Write one LSM6DSL register.
 * @param reg Register address.
 * @param value Byte to write.
 * @return @c true when the HAL transaction succeeds.
 */
static bool sensor_reg_write(uint8_t reg, uint8_t value) {
    return HAL_I2C_Mem_Write(&hi2c2,
                             LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW,
                             reg,
                             I2C_MEMADD_SIZE_8BIT,
                             &value,
                             1u,
                             SENSOR_IO_TIMEOUT_MS) == HAL_OK;
}

/**
 * @brief Configure a pulsed accelerometer data-ready signal on INT1.
 * @return @c true when both register writes succeed.
 */
static bool sensor_enable_data_ready_interrupt(void) {
    return sensor_reg_write(LSM6DSL_ACC_GYRO_DRDY_PULSE_CFG_G, LSM6DSL_DRDY_PULSED_BIT) &&
           sensor_reg_write(LSM6DSL_ACC_GYRO_INT1_CTRL, LSM6DSL_INT1_DRDY_XL_BIT);
}

/**
 * @brief Decode one little-endian signed 16-bit sensor value.
 * @param bytes Two bytes in low-byte, high-byte order.
 * @return Decoded signed value.
 */
static int16_t decode_int16(const uint8_t *bytes) {
    return (int16_t)(((uint16_t)bytes[1] << 8u) | bytes[0]);
}

bool sensor_device_set_mode(sensor_mode_t mode) {
    uint8_t odr;
    uint16_t low_power;
    uint8_t ctrl1;
    uint8_t ctrl2;

    switch (mode) {
        case SENSOR_MODE_LOW:
            odr = LSM6DSL_ODR_52Hz;
            low_power = 1u;
            break;
        case SENSOR_MODE_NORMAL:
            odr = LSM6DSL_ODR_104Hz;
            low_power = 0u;
            break;
        case SENSOR_MODE_HIGH:
            odr = LSM6DSL_ODR_416Hz;
            low_power = 0u;
            break;
        default:
            return false;
    }

    if (!sensor_reg_read(LSM6DSL_ACC_GYRO_CTRL1_XL, &ctrl1) ||
        !sensor_reg_read(LSM6DSL_ACC_GYRO_CTRL2_G, &ctrl2)) {
        return false;
    }

    BSP_ACCELERO_LowPower(low_power);
    BSP_GYRO_LowPower(low_power);

    ctrl1 = (uint8_t)((ctrl1 & (uint8_t)~LSM6DSL_ODR_BITPOSITION) | odr);
    ctrl2 = (uint8_t)((ctrl2 & (uint8_t)~LSM6DSL_ODR_BITPOSITION) | odr);

    return sensor_reg_write(LSM6DSL_ACC_GYRO_CTRL1_XL, ctrl1) &&
           sensor_reg_write(LSM6DSL_ACC_GYRO_CTRL2_G, ctrl2);
}

bool sensor_device_init(void) {
    return (BSP_ACCELERO_Init() == ACCELERO_OK) && (BSP_GYRO_Init() == GYRO_OK) &&
           sensor_device_set_mode(SENSOR_MODE_NORMAL) && sensor_enable_data_ready_interrupt();
}

bool sensor_device_read(sensor_sample_t *sample) {
    uint8_t raw[12];

    if (sample == 0) {
        return false;
    }

    if (HAL_I2C_Mem_Read(&hi2c2,
                         LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW,
                         LSM6DSL_ACC_GYRO_OUTX_L_G,
                         I2C_MEMADD_SIZE_8BIT,
                         raw,
                         sizeof(raw),
                         SENSOR_IO_TIMEOUT_MS) != HAL_OK) {
        return false;
    }

    for (uint32_t axis = 0u; axis < 3u; ++axis) {
        int16_t gyro_raw = decode_int16(&raw[axis * 2u]);
        int16_t accel_raw = decode_int16(&raw[6u + axis * 2u]);

        sample->angular_rate_dps[axis] =
            ((float)gyro_raw * LSM6DSL_GYRO_SENSITIVITY_2000DPS) / 1000.0f;
        sample->acceleration_g[axis] = ((float)accel_raw * LSM6DSL_ACC_SENSITIVITY_2G) / 1000.0f;
    }

    return true;
}

bool sensor_device_read_status(sensor_device_status_t *status) {
    return (status != 0) && sensor_reg_read(LSM6DSL_ACC_GYRO_CTRL1_XL, &status->ctrl1_xl) &&
           sensor_reg_read(LSM6DSL_ACC_GYRO_CTRL2_G, &status->ctrl2_g);
}

bool sensor_device_reset(void) {
    uint8_t ctrl3;
    uint32_t start;

    if (!sensor_reg_read(LSM6DSL_ACC_GYRO_CTRL3_C, &ctrl3) ||
        !sensor_reg_write(LSM6DSL_ACC_GYRO_CTRL3_C, ctrl3 | LSM6DSL_SW_RESET_BIT)) {
        return false;
    }

    start = HAL_GetTick();

    do {
        if (!sensor_reg_read(LSM6DSL_ACC_GYRO_CTRL3_C, &ctrl3)) {
            return false;
        }

        if ((ctrl3 & LSM6DSL_SW_RESET_BIT) == 0u) {
            return sensor_device_init();
        }

        os_delay(1u);
    } while ((HAL_GetTick() - start) < SENSOR_RESET_TIMEOUT_MS);

    return false;
}

#endif /* PROJECT == PROJECT_SENSOR */
