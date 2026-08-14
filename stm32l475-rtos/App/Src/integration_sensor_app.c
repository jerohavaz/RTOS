/**
 * @file integration_sensor_app.c
 * @brief Interrupt-driven sensor acquisition and command orchestration.
 * @author Jerome
 * @author Martin
 *
 * The data-ready interrupt releases a binary semaphore. The sensor task reads
 * one sample per observed interrupt, accumulates samples, and publishes a
 * batch on fixed 100 ms deadlines. Shell commands share the same wake-up path
 * and are executed by the sensor task so device access stays serialized.
 */

#include "integration_sensor_app.h"

#include "project.h"

#if PROJECT == PROJECT_SENSOR

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "integration_sensor_device.h"
#include "integration_sensor_internal.h"
#include "integration_sensor_output.h"
#include "integration_sensor_types.h"
#include "integration_test.h"
#include "integration_tests.h"
#include "main.h"
#include "os_config.h"
#include "os_queue.h"
#include "os_sem.h"
#include "os_task.h"

#define SENSOR_COMMAND_QUEUE_ID       1u   /**< TeSSLa-visible command queue ID. */
#define SENSOR_COMMAND_QUEUE_CAPACITY 8u   /**< Maximum pending shell commands. */
#define SENSOR_BATCH_PERIOD_MS        100u /**< Period between output batches. */
#define SENSOR_TASK_PRIORITY          6u   /**< Acquisition task priority. */
#define SENSOR_OUTPUT_TASK_PRIORITY   5u   /**< UART output task priority. */

/** @brief Binary wake-up semaphore released by commands and data-ready IRQs. */
static os_sem_t sensor_data_ready_sem;

/** @brief Shell-to-sensor command queue. */
static os_queue_t sensor_command_queue;

/** @brief Backing storage for @ref sensor_command_queue. */
static app_sensor_command_t sensor_command_storage[SENSOR_COMMAND_QUEUE_CAPACITY];

/** @brief Whether the sensor completed its most recent initialization. */
static bool sensor_available;

/** @brief Number of LSM6DSL data-ready interrupts received. */
static volatile uint32_t sensor_interrupt_count;

/** @brief Number of sensor samples read successfully. */
static uint32_t sensor_sample_count;

/** @brief Number of samples discarded with an output batch. */
static uint32_t sensor_queue_drop_count;

/** @brief Debugger-visible count of runtime sensor-application errors. */
volatile uint32_t test_error_count;

void sensor_app_record_error(void) {
    ++test_error_count;
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
}

/**
 * @brief Queue a constant response string and record failure if it cannot be queued.
 * @param text Null-terminated response or error text.
 */
static void post_text(const char *text) {
    if (sensor_output_post_text("%s", text) != OS_OK) {
        sensor_app_record_error();
    }
}

/** @brief Read and queue the device registers and runtime counters. */
static void sensor_report_status(void) {
    sensor_device_status_t status;

    if (!sensor_device_read_status(&status)) {
        post_text("ERROR,SENSOR,STATUS_READ_FAILED\r\n");
        return;
    }

    if (sensor_output_post_text("RESP,STATUS,"
                                "CTRL1_XL=0x%02X,"
                                "CTRL2_G=0x%02X,"
                                "IRQ=%lu,"
                                "READ=%lu,"
                                "DROPPED=%lu\r\n",
                                status.ctrl1_xl,
                                status.ctrl2_g,
                                (unsigned long)sensor_interrupt_count,
                                (unsigned long)sensor_sample_count,
                                (unsigned long)sensor_queue_drop_count) != OS_OK) {
        sensor_app_record_error();
    }
}

/**
 * @brief Apply a sensor mode and queue its asynchronous result.
 * @param mode Device mode to apply.
 * @param name Uppercase mode name used in the UART response.
 */
static void sensor_set_mode(sensor_mode_t mode, const char *name) {
    if (sensor_device_set_mode(mode)) {
        if (sensor_output_post_text("RESP,MODE,%s,OK\r\n", name) != OS_OK) {
            sensor_app_record_error();
        }
    } else if (sensor_output_post_text("ERROR,SENSOR,MODE_%s_FAILED\r\n", name) != OS_OK) {
        sensor_app_record_error();
    }
}

/** @brief Drain and execute every currently queued sensor command. */
static void sensor_process_commands(void) {
    app_sensor_command_t command;

    while (os_queue_recv(&sensor_command_queue, &command, OS_NO_WAIT) == OS_OK) {
        switch (command) {
            case APP_SENSOR_CMD_MODE_LOW:
                sensor_set_mode(SENSOR_MODE_LOW, "LOW");
                break;
            case APP_SENSOR_CMD_MODE_NORMAL:
                sensor_set_mode(SENSOR_MODE_NORMAL, "NORMAL");
                break;
            case APP_SENSOR_CMD_MODE_HIGH:
                sensor_set_mode(SENSOR_MODE_HIGH, "HIGH");
                break;
            case APP_SENSOR_CMD_RESET:
                sensor_available = sensor_device_reset();
                post_text(sensor_available ? "RESP,RESET,OK\r\n" : "ERROR,SENSOR,RESET_FAILED\r\n");
                break;
            case APP_SENSOR_CMD_STATUS:
                sensor_report_status();
                break;
            default:
                post_text("ERROR,SENSOR,UNKNOWN_COMMAND\r\n");
                break;
        }
    }
}

/**
 * @brief Add one reading to an output batch.
 * @param[in,out] batch Batch accumulator to update.
 * @param sample Reading to add.
 */
static void batch_add(sensor_sample_batch_t *batch, const sensor_sample_t *sample) {
    for (uint32_t axis = 0u; axis < 3u; ++axis) {
        batch->sum.acceleration_g[axis] += sample->acceleration_g[axis];
        batch->sum.angular_rate_dps[axis] += sample->angular_rate_dps[axis];
    }

    ++batch->count;
}

/**
 * @brief Queue a non-empty batch and clear its accumulator.
 * @param[in,out] batch Batch to publish and reset.
 */
static void batch_publish(sensor_sample_batch_t *batch) {
    if (batch->count == 0u) {
        return;
    }

    if (sensor_output_post_batch(batch) != OS_OK) {
        sensor_queue_drop_count += batch->count;
        sensor_app_record_error();
    }

    memset(batch, 0, sizeof(*batch));
}

/**
 * @brief Test a wrapping 32-bit millisecond deadline.
 * @param now Current tick value.
 * @param deadline Deadline to compare against.
 * @return @c true when @p deadline has been reached or passed.
 */
static bool batch_deadline_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

/**
 * @brief Publish a due batch and advance to the next future deadline.
 * @param[in,out] batch Current batch accumulator.
 * @param[in,out] deadline Current deadline, advanced in 100 ms increments.
 * @param now Current tick value.
 */
static void batch_publish_due(sensor_sample_batch_t *batch, uint32_t *deadline, uint32_t now) {
    if (!batch_deadline_reached(now, *deadline)) {
        return;
    }

    batch_publish(batch);

    do {
        *deadline += SENSOR_BATCH_PERIOD_MS;
    } while (batch_deadline_reached(now, *deadline));
}

/**
 * @brief Run acquisition, command processing, and fixed-deadline batching.
 *
 * The semaphore timeout is the time remaining until the next batch deadline;
 * therefore low sample rates do not extend the output period. The task never
 * returns.
 */
static void sensor_task(void) {
    sensor_sample_batch_t batch = { 0 };
    uint32_t batch_deadline = HAL_GetTick() + SENSOR_BATCH_PERIOD_MS;
    uint32_t handled_interrupt_count = 0u;

    if (!sensor_available) {
        post_text("ERROR,SENSOR,INITIALIZATION_FAILED\r\n");
    }

    while (1) {
        uint32_t now = HAL_GetTick();
        batch_publish_due(&batch, &batch_deadline, now);

        uint32_t wait_ticks = batch_deadline - now;
        os_status_t wait_status = os_sem_acquire(&sensor_data_ready_sem, wait_ticks);

        now = HAL_GetTick();
        batch_publish_due(&batch, &batch_deadline, now);

        if (wait_status == OS_ERR_TIMEOUT) {
            continue;
        }

        if (wait_status != OS_OK) {
            sensor_app_record_error();
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

        sensor_sample_t sample;

        if (!sensor_device_read(&sample)) {
            post_text("ERROR,SENSOR,DATA_READ_FAILED\r\n");
            continue;
        }

        ++sensor_sample_count;
        batch_add(&batch, &sample);
    }
}

os_status_t app_sensor_command_submit(app_sensor_command_t command) {
    os_status_t status = os_queue_send(&sensor_command_queue, &command, OS_NO_WAIT);

    if (status != OS_OK) {
        return status;
    }

    status = os_sem_release(&sensor_data_ready_sem);
    return (status == OS_ERR_FULL) ? OS_OK : status;
}

/**
 * @brief Handle the board data-ready external interrupt callback.
 * @param gpio_pin Pin reported by the HAL EXTI dispatcher.
 */
void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin) {
    if (gpio_pin != LSM6DSL_INT1_EXTI11_Pin) {
        return;
    }

    ++sensor_interrupt_count;

    os_status_t status = os_sem_release(&sensor_data_ready_sem);

    if ((status != OS_OK) && (status != OS_ERR_FULL)) {
        sensor_app_record_error();
    }
}

/**
 * @brief Initialize sensor objects, hardware, and worker tasks.
 *
 * Device initialization temporarily enables interrupts because the STM32 BSP
 * I2C implementation requires interrupt delivery. The previous PRIMASK state
 * is restored afterward.
 */
void integration_sensor_app_init(void) {
    sensor_available = false;
    sensor_interrupt_count = 0u;
    sensor_sample_count = 0u;
    sensor_queue_drop_count = 0u;
    test_error_count = 0u;

    integration_test_check(os_sem_init(&sensor_data_ready_sem, 0u, 1u) == OS_OK);
    integration_test_check(os_queue_init(&sensor_command_queue,
                                         SENSOR_COMMAND_QUEUE_ID,
                                         sensor_command_storage,
                                         sizeof(sensor_command_storage[0]),
                                         SENSOR_COMMAND_QUEUE_CAPACITY) == OS_OK);
    integration_test_check(sensor_output_init() == OS_OK);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, OS_KERNEL_INTERRUPT_PRIORITY, 0u);

    uint32_t previous_primask = __get_PRIMASK();
    __enable_irq();
    sensor_available = sensor_device_init();

    if (previous_primask != 0u) {
        __disable_irq();
    }

    integration_test_check(sensor_available);
    integration_test_check(os_task_create(sensor_task, SENSOR_TASK_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(sensor_output_task, SENSOR_OUTPUT_TASK_PRIORITY) ==
                           OS_OK);
}

#endif /* PROJECT == PROJECT_SENSOR */
