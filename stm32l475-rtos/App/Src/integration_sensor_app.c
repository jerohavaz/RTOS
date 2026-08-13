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

#define SENSOR_COMMAND_QUEUE_ID       1u
#define SENSOR_COMMAND_QUEUE_CAPACITY 8u
#define SENSOR_BATCH_PERIOD_MS        100u
#define SENSOR_TASK_PRIORITY          6u
#define SENSOR_OUTPUT_TASK_PRIORITY   5u

static os_sem_t sensor_data_ready_sem;
static os_queue_t sensor_command_queue;
static app_sensor_command_t sensor_command_storage[SENSOR_COMMAND_QUEUE_CAPACITY];

static bool sensor_available;
static volatile uint32_t sensor_interrupt_count;
static uint32_t sensor_sample_count;
static uint32_t sensor_queue_drop_count;

volatile uint32_t test_error_count;

void sensor_app_record_error(void) {
    ++test_error_count;
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
}

static void post_text(const char *text) {
    if (sensor_output_post_text("%s", text) != OS_OK) {
        sensor_app_record_error();
    }
}

static void sensor_report_status(void) {
    sensor_device_status_t status;

    if (!sensor_device_read_status(&status)) {
        post_text("ERROR,SENSOR,STATUS_READ_FAILED\r\nCLI>");
        return;
    }

    if (sensor_output_post_text("RESP,STATUS,"
                                "CTRL1_XL=0x%02X,"
                                "CTRL2_G=0x%02X,"
                                "IRQ=%lu,"
                                "READ=%lu,"
                                "DROPPED=%lu\r\nCLI>",
                                status.ctrl1_xl,
                                status.ctrl2_g,
                                (unsigned long)sensor_interrupt_count,
                                (unsigned long)sensor_sample_count,
                                (unsigned long)sensor_queue_drop_count) != OS_OK) {
        sensor_app_record_error();
    }
}

static void sensor_set_mode(sensor_mode_t mode, const char *name) {
    if (sensor_device_set_mode(mode)) {
        if (sensor_output_post_text("RESP,MODE,%s,OK\r\nCLI>", name) != OS_OK) {
            sensor_app_record_error();
        }
    } else if (sensor_output_post_text("ERROR,SENSOR,MODE_%s_FAILED\r\nCLI>", name) != OS_OK) {
        sensor_app_record_error();
    }
}

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
                post_text(sensor_available ? "RESP,RESET,OK\r\nCLI>"
                                           : "ERROR,SENSOR,RESET_FAILED\r\nCLI>");
                break;
            case APP_SENSOR_CMD_STATUS:
                sensor_report_status();
                break;
            default:
                post_text("ERROR,SENSOR,UNKNOWN_COMMAND\r\nCLI>");
                break;
        }
    }
}

static void batch_add(sensor_sample_batch_t *batch, const sensor_sample_t *sample) {
    for (uint32_t axis = 0u; axis < 3u; ++axis) {
        batch->sum.acceleration_g[axis] += sample->acceleration_g[axis];
        batch->sum.angular_rate_dps[axis] += sample->angular_rate_dps[axis];
    }

    ++batch->count;
}

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

static bool batch_deadline_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

static void batch_publish_due(sensor_sample_batch_t *batch, uint32_t *deadline, uint32_t now) {
    if (!batch_deadline_reached(now, *deadline)) {
        return;
    }

    batch_publish(batch);

    do {
        *deadline += SENSOR_BATCH_PERIOD_MS;
    } while (batch_deadline_reached(now, *deadline));
}

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
