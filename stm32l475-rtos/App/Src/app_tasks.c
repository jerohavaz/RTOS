#include "app_tasks.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "os_config.h"
#include "os_delay.h"
#include "os_queue.h"
#include "os_sem.h"
#include "os_task.h"
#include "shell.h"
#include "stm32l475e_iot01_accelero.h"
#include "stm32l475e_iot01_gyro.h"
#include "trace.h"

#define UART_QUEUE_MESSAGE_COUNT   64u
#define SENSOR_COMMAND_QUEUE_COUNT 8u
#define UART_TEXT_LENGTH           96u
#define UART_PERIOD_MS             100u

#define LSM6DSL_INT1_DRDY_XL_BIT 0x01u
#define LSM6DSL_DRDY_PULSED_BIT  0x80u
#define LSM6DSL_SW_RESET_BIT     0x01u

typedef struct {
    float acceleration_g[3];
    float angular_rate_dps[3];
} sensor_sample_t;

typedef enum { UART_MESSAGE_SAMPLE = 0, UART_MESSAGE_TEXT } uart_message_type_t;

typedef struct {
    uart_message_type_t type;

    union {
        sensor_sample_t sample;
        char text[UART_TEXT_LENGTH];
    } payload;
} uart_message_t;

/* RTOS objects */
static os_sem_t sensor_data_ready_sem;
static os_queue_t sensor_command_queue;
static os_queue_t uart_queue;

/* Queue storage */
static app_sensor_command_t sensor_command_storage[SENSOR_COMMAND_QUEUE_COUNT];

static uart_message_t uart_queue_storage[UART_QUEUE_MESSAGE_COUNT];

static bool sensor_available;

static volatile uint32_t sensor_interrupt_count;
static uint32_t sensor_sample_count;
static uint32_t sensor_queue_drop_count;

/* HAL handles generated in main.c */
extern I2C_HandleTypeDef hi2c2;
extern UART_HandleTypeDef huart1;

volatile uint32_t test_error_count;

static void test_fail(void) {
    test_error_count++;

    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
}

static void expect_status(os_status_t actual, os_status_t expected) {
    if (actual != expected) {
        test_fail();
    }
}

static HAL_StatusTypeDef sensor_reg_read(uint8_t reg, uint8_t *value) {
    return HAL_I2C_Mem_Read(
        &hi2c2, LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, reg, I2C_MEMADD_SIZE_8BIT, value, 1u, 100u);
}

static HAL_StatusTypeDef sensor_reg_write(uint8_t reg, uint8_t value) {
    return HAL_I2C_Mem_Write(
        &hi2c2, LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, reg, I2C_MEMADD_SIZE_8BIT, &value, 1u, 100u);
}

static void uart_queue_text(const char *format, ...) {
    uart_message_t message = { .type = UART_MESSAGE_TEXT };

    va_list arguments;

    va_start(arguments, format);

    vsnprintf(message.payload.text, sizeof(message.payload.text), format, arguments);

    va_end(arguments);

    if (os_queue_send(&uart_queue, &message, 10u) != OS_OK) {
        test_fail();
    }
}

static HAL_StatusTypeDef sensor_enable_data_ready_interrupt(void) {
    if (sensor_reg_write(LSM6DSL_ACC_GYRO_DRDY_PULSE_CFG_G, LSM6DSL_DRDY_PULSED_BIT) != HAL_OK) {
        return HAL_ERROR;
    }

    return sensor_reg_write(LSM6DSL_ACC_GYRO_INT1_CTRL, LSM6DSL_INT1_DRDY_XL_BIT);
}

static HAL_StatusTypeDef sensor_set_data_rate(uint8_t odr) {
    uint8_t ctrl1;
    uint8_t ctrl2;

    if (sensor_reg_read(LSM6DSL_ACC_GYRO_CTRL1_XL, &ctrl1) != HAL_OK) {
        return HAL_ERROR;
    }

    if (sensor_reg_read(LSM6DSL_ACC_GYRO_CTRL2_G, &ctrl2) != HAL_OK) {
        return HAL_ERROR;
    }

    /*
     * Change only ODR bits 7:4.
     * Preserve full-scale settings in bits 3:0.
     */
    ctrl1 = (uint8_t)((ctrl1 & (uint8_t)~LSM6DSL_ODR_BITPOSITION) | odr);

    ctrl2 = (uint8_t)((ctrl2 & (uint8_t)~LSM6DSL_ODR_BITPOSITION) | odr);

    if (sensor_reg_write(LSM6DSL_ACC_GYRO_CTRL1_XL, ctrl1) != HAL_OK) {
        return HAL_ERROR;
    }

    if (sensor_reg_write(LSM6DSL_ACC_GYRO_CTRL2_G, ctrl2) != HAL_OK) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef sensor_apply_profile(app_sensor_command_t profile) {
    uint8_t odr;
    uint16_t low_power;

    switch (profile) {
        case APP_SENSOR_CMD_MODE_LOW:
            odr = LSM6DSL_ODR_52Hz;
            low_power = 1u;
            break;

        case APP_SENSOR_CMD_MODE_NORMAL:
            odr = LSM6DSL_ODR_104Hz;
            low_power = 0u;
            break;

        case APP_SENSOR_CMD_MODE_HIGH:
            odr = LSM6DSL_ODR_416Hz;
            low_power = 0u;
            break;

        default:
            return HAL_ERROR;
    }

    BSP_ACCELERO_LowPower(low_power);
    BSP_GYRO_LowPower(low_power);

    return sensor_set_data_rate(odr);
}

static HAL_StatusTypeDef sensor_hardware_init(void) {
    if (BSP_ACCELERO_Init() != ACCELERO_OK) {
        return HAL_ERROR;
    }

    if (BSP_GYRO_Init() != GYRO_OK) {
        return HAL_ERROR;
    }

    if (sensor_apply_profile(APP_SENSOR_CMD_MODE_NORMAL) != HAL_OK) {
        return HAL_ERROR;
    }

    return sensor_enable_data_ready_interrupt();
}

static int16_t decode_int16(const uint8_t *bytes) {
    return (int16_t)(((uint16_t)bytes[1] << 8u) | bytes[0]);
}

static HAL_StatusTypeDef sensor_read_sample(sensor_sample_t *sample) {
    uint8_t raw[12];

    /*
     * Read gyro and accelerometer in one transaction:
     *
     * 0x22–0x27: gyro XYZ
     * 0x28–0x2D: accelerometer XYZ
     */
    if (HAL_I2C_Mem_Read(&hi2c2,
                         LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW,
                         LSM6DSL_ACC_GYRO_OUTX_L_G,
                         I2C_MEMADD_SIZE_8BIT,
                         raw,
                         sizeof(raw),
                         100u) != HAL_OK) {
        return HAL_ERROR;
    }

    for (uint32_t axis = 0u; axis < 3u; axis++) {
        int16_t gyro_raw = decode_int16(&raw[axis * 2u]);

        int16_t accel_raw = decode_int16(&raw[6u + axis * 2u]);

        /*
         * BSP gyroscope default: ±2000 degrees/second.
         * Sensitivity is expressed as mdps/LSB.
         */
        sample->angular_rate_dps[axis] =
            ((float)gyro_raw * LSM6DSL_GYRO_SENSITIVITY_2000DPS) / 1000.0f;

        /*
         * BSP accelerometer default: ±2 g.
         * Sensitivity is expressed as mg/LSB.
         */
        sample->acceleration_g[axis] = ((float)accel_raw * LSM6DSL_ACC_SENSITIVITY_2G) / 1000.0f;
    }

    return HAL_OK;
}

static void sensor_report_status(void) {
    uint8_t ctrl1;
    uint8_t ctrl2;

    if (sensor_reg_read(LSM6DSL_ACC_GYRO_CTRL1_XL, &ctrl1) != HAL_OK) {
        uart_queue_text("ERROR,SENSOR,CTRL1_READ_FAILED\r\nCLI>");

        return;
    }

    if (sensor_reg_read(LSM6DSL_ACC_GYRO_CTRL2_G, &ctrl2) != HAL_OK) {
        uart_queue_text("ERROR,SENSOR,CTRL2_READ_FAILED\r\nCLI>");

        return;
    }

    uart_queue_text("RESP,STATUS,"
                    "CTRL1_XL=0x%02X,"
                    "CTRL2_G=0x%02X,"
                    "IRQ=%lu,"
                    "READ=%lu,"
                    "DROPPED=%lu\r\nCLI>",
                    ctrl1,
                    ctrl2,
                    (unsigned long)sensor_interrupt_count,
                    (unsigned long)sensor_sample_count,
                    (unsigned long)sensor_queue_drop_count);
}

static void sensor_reset(void) {
    uint8_t ctrl3;
    uint32_t start;

    if (sensor_reg_read(LSM6DSL_ACC_GYRO_CTRL3_C, &ctrl3) != HAL_OK) {
        uart_queue_text("ERROR,SENSOR,RESET_READ_FAILED\r\nCLI>");

        return;
    }

    ctrl3 |= LSM6DSL_SW_RESET_BIT;

    if (sensor_reg_write(LSM6DSL_ACC_GYRO_CTRL3_C, ctrl3) != HAL_OK) {
        uart_queue_text("ERROR,SENSOR,RESET_WRITE_FAILED\r\nCLI>");

        return;
    }

    start = HAL_GetTick();

    do {
        if (sensor_reg_read(LSM6DSL_ACC_GYRO_CTRL3_C, &ctrl3) != HAL_OK) {
            uart_queue_text("ERROR,SENSOR,RESET_POLL_FAILED\r\nCLI>");

            return;
        }

        if ((ctrl3 & LSM6DSL_SW_RESET_BIT) == 0u) {
            break;
        }

        (void)os_delay(1u);

    } while ((HAL_GetTick() - start) < 20u);

    if ((ctrl3 & LSM6DSL_SW_RESET_BIT) != 0u) {
        uart_queue_text("ERROR,SENSOR,RESET_TIMEOUT\r\nCLI>");

        return;
    }

    sensor_available = (sensor_hardware_init() == HAL_OK);

    if (sensor_available) {
        uart_queue_text("RESP,RESET,OK\r\nCLI>");
    } else {
        uart_queue_text("ERROR,SENSOR,REINIT_FAILED\r\nCLI>");
    }
}

static void sensor_process_commands(void) {
    app_sensor_command_t command;

    while (os_queue_recv(&sensor_command_queue, &command, OS_NO_WAIT) == OS_OK) {
        switch (command) {
            case APP_SENSOR_CMD_MODE_LOW:
            case APP_SENSOR_CMD_MODE_NORMAL:
            case APP_SENSOR_CMD_MODE_HIGH: {
                const char *name = (command == APP_SENSOR_CMD_MODE_LOW)      ? "LOW"
                                   : (command == APP_SENSOR_CMD_MODE_NORMAL) ? "NORMAL"
                                                                             : "HIGH";

                if (sensor_apply_profile(command) == HAL_OK) {
                    uart_queue_text("RESP,MODE,%s,OK\r\nCLI>", name);
                } else {
                    uart_queue_text("ERROR,SENSOR,MODE_%s_FAILED\r\nCLI>", name);
                }
                break;
            }

            case APP_SENSOR_CMD_RESET:
                sensor_reset();
                break;

            case APP_SENSOR_CMD_STATUS:
                sensor_report_status();
                break;

            default:
                uart_queue_text("ERROR,SENSOR,UNKNOWN_COMMAND\r\nCLI>");
                break;
        }
    }
}

static void sensor_task(void) {
    uart_message_t message = { .type = UART_MESSAGE_SAMPLE };
    uint32_t handled_interrupt_count = 0u;

    if (!sensor_available) {
        uart_queue_text("ERROR,SENSOR,INITIALIZATION_FAILED\r\n");
    }

    while (1) {
        if (os_sem_acquire(&sensor_data_ready_sem, OS_WAIT_FOREVER) != OS_OK) {
            test_fail();
            continue;
        }

        sensor_process_commands();

        uint32_t interrupt_count = sensor_interrupt_count;

        if (interrupt_count == handled_interrupt_count) {
            continue;
        }

        handled_interrupt_count = interrupt_count;

        if (!sensor_available) {
            continue;
        }

        trace_sensor_read();

        if (sensor_read_sample(&message.payload.sample) != HAL_OK) {
            uart_queue_text("ERROR,SENSOR,DATA_READ_FAILED\r\n");

            continue;
        }

        sensor_sample_count++;

        if (os_queue_send(&uart_queue, &message, OS_NO_WAIT) != OS_OK) {
            sensor_queue_drop_count++;
            test_fail();
        }
    }
}

static int32_t to_milli(float value) {
    float scaled = value * 1000.0f;

    return (int32_t)(scaled + ((scaled >= 0.0f) ? 0.5f : -0.5f));
}

static void format_milli(char *output, size_t output_size, int32_t milli) {
    uint32_t magnitude;

    if (milli < 0) {
        magnitude = (uint32_t)(-(int64_t)milli);

        snprintf(output,
                 output_size,
                 "-%lu.%03lu",
                 (unsigned long)(magnitude / 1000u),
                 (unsigned long)(magnitude % 1000u));
    } else {
        magnitude = (uint32_t)milli;

        snprintf(output,
                 output_size,
                 "%lu.%03lu",
                 (unsigned long)(magnitude / 1000u),
                 (unsigned long)(magnitude % 1000u));
    }
}

static void uart_send_average(const sensor_sample_t *sum, uint32_t sample_count) {
    char values[6][20];
    char line[160];

    if ((sample_count == 0u) || !is_stream_enabled()) {
        return;
    }

    for (uint32_t axis = 0u; axis < 3u; axis++) {
        format_milli(values[axis],
                     sizeof(values[axis]),
                     to_milli(sum->acceleration_g[axis] / (float)sample_count));

        format_milli(values[axis + 3u],
                     sizeof(values[axis + 3u]),
                     to_milli(sum->angular_rate_dps[axis] / (float)sample_count));
    }

    int length = snprintf(line,
                          sizeof(line),
                          "DATA,%s,%s,%s,%s,%s,%s,%lu\r\n",
                          values[0],
                          values[1],
                          values[2],
                          values[3],
                          values[4],
                          values[5],
                          (unsigned long)sample_count);

    if ((length > 0) && (length < (int)sizeof(line))) {
        HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, HAL_MAX_DELAY);

        trace_transmission_complete();
    } else {
        test_fail();
    }
}

static void uart_process_message(const uart_message_t *message,
                                 sensor_sample_t *sum,
                                 uint32_t *sample_count) {
    if (message->type == UART_MESSAGE_SAMPLE) {
        for (uint32_t axis = 0u; axis < 3u; axis++) {
            sum->acceleration_g[axis] += message->payload.sample.acceleration_g[axis];
            sum->angular_rate_dps[axis] += message->payload.sample.angular_rate_dps[axis];
        }

        (*sample_count)++;
    } else if (message->type == UART_MESSAGE_TEXT) {
        HAL_UART_Transmit(&huart1,
                          (uint8_t *)message->payload.text,
                          (uint16_t)strlen(message->payload.text),
                          HAL_MAX_DELAY);
    }
}

static void uart_task(void) {
    uart_message_t message;
    sensor_sample_t sum = { 0 };
    uint32_t sample_count = 0u;
    uint32_t next_transmission = HAL_GetTick() + UART_PERIOD_MS;

    HAL_UART_Transmit(&huart1,
                      (uint8_t *)"TYPE,"
                                 "ax_g,ay_g,az_g,"
                                 "gx_dps,gy_dps,gz_dps,"
                                 "samples\r\n",
                      sizeof("TYPE,"
                             "ax_g,ay_g,az_g,"
                             "gx_dps,gy_dps,gz_dps,"
                             "samples\r\n") -
                          1u,
                      HAL_MAX_DELAY);

    shell_init();

    while (1) {
        shell_update();

        /*
         * Drain every currently queued message.
         *
         * Queue reception is continuous. Only UART transmission
         * is limited to one record every 100 ms.
         */
        os_status_t status;

        while ((status = os_queue_recv(&uart_queue, &message, OS_NO_WAIT)) == OS_OK) {
            uart_process_message(&message, &sum, &sample_count);
        }

        if (status != OS_ERR_WOULD_BLOCK) {
            test_fail();
        }

        uint32_t now = HAL_GetTick();

        if ((int32_t)(now - next_transmission) >= 0) {
            uart_send_average(&sum, sample_count);
            memset(&sum, 0, sizeof(sum));
            sample_count = 0u;

            next_transmission += UART_PERIOD_MS;

            if ((int32_t)(now - next_transmission) >= 0) {
                next_transmission = now + UART_PERIOD_MS;
            }
        }

        /*
         * UART has lower priority than the sensor task, but this
         * delay also prevents unnecessary busy polling.
         */
        (void)os_delay(1u);
    }
}

os_status_t app_sensor_command_submit(app_sensor_command_t command) {
    os_status_t status;

    status = os_queue_send(&sensor_command_queue, &command, OS_NO_WAIT);

    if (status != OS_OK) {
        return status;
    }

    /*
     * Commands wake the sensor task using the same semaphore.
     * The interrupt counter distinguishes this from a DRDY wake-up.
     */
    status = os_sem_release(&sensor_data_ready_sem);

    if (status == OS_ERR_FULL) {
        return OS_OK;
    }

    return status;
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin) {
    if (gpio_pin != LSM6DSL_INT1_EXTI11_Pin) {
        return;
    }

    sensor_interrupt_count++;

    os_status_t status = os_sem_release(&sensor_data_ready_sem);

    /*
     * A full binary semaphore already represents a pending
     * sensor event.
     */
    if ((status != OS_OK) && (status != OS_ERR_FULL)) {
        test_fail();
    }
}

void app_tasks_init(void) {
    uint32_t previous_primask;

    sensor_available = false;

    sensor_interrupt_count = 0u;
    sensor_sample_count = 0u;
    sensor_queue_drop_count = 0u;
    test_error_count = 0u;

    expect_status(os_sem_init(&sensor_data_ready_sem, 0u, 1u), OS_OK);

    expect_status(os_queue_init(&sensor_command_queue,
                                sensor_command_storage,
                                sizeof(sensor_command_storage[0]),
                                SENSOR_COMMAND_QUEUE_COUNT),
                  OS_OK);

    expect_status(os_queue_init(&uart_queue,
                                uart_queue_storage,
                                sizeof(uart_queue_storage[0]),
                                UART_QUEUE_MESSAGE_COUNT),
                  OS_OK);

    /*
     * EXTI invokes an RTOS semaphore function. It must not run
     * above the kernel interrupt-priority ceiling.
     */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, OS_KERNEL_INTERRUPT_PRIORITY, 0u);

    /*
     * os_init() disables interrupts. Temporarily enable them so
     * HAL I2C timeout handling still has a running HAL tick.
     */
    previous_primask = __get_PRIMASK();

    __enable_irq();

    /*
     * Initial sensor configuration occurs before task startup. TODO: MAYBE PUT INTO MAIN
     */
    sensor_available = (sensor_hardware_init() == HAL_OK);

    if (previous_primask != 0u) {
        __disable_irq();
    }

    /*
     * Sensor acquisition is time-critical and therefore has the
     * higher task priority.
     */
    expect_status(os_task_create(sensor_task, 6u), OS_OK);

    expect_status(os_task_create(uart_task, 5u), OS_OK);
}