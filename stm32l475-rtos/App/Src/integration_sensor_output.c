/**
 * @file integration_sensor_output.c
 * @brief Queue-backed UART serialization for sensor data and responses.
 * @author Jerome
 * @author Martin
 *
 * A single output task owns normal UART transmission. Producers queue either
 * a completed sample batch or formatted text, preventing interleaved records
 * from the acquisition and shell paths.
 */

#include "integration_sensor_output.h"

#include "project.h"

#if PROJECT == PROJECT_SENSOR

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "integration_sensor_internal.h"
#include "integration_sensor_shell.h"
#include "main.h"
#include "os_queue.h"
#include "trace.h"

#define SENSOR_OUTPUT_QUEUE_ID       2u  /**< TeSSLa-visible output queue ID. */
#define SENSOR_OUTPUT_QUEUE_CAPACITY 96u /**< Maximum queued output messages. */
#define SENSOR_OUTPUT_TEXT_LENGTH    96u /**< Text bytes stored per message. */
#define SENSOR_OUTPUT_POST_TIMEOUT   10u /**< Text-producer queue timeout. */
#define SENSOR_SHELL_POLL_TICKS      10u /**< Maximum interval between shell polls. */

/** @brief Payload discriminator for output queue entries. */
typedef enum {
    SENSOR_OUTPUT_BATCH = 0, /**< Averaged sensor stream record. */
    SENSOR_OUTPUT_TEXT       /**< Shell response or error text. */
} sensor_output_message_type_t;

/** @brief One message serialized by the output task. */
typedef struct {
    sensor_output_message_type_t type; /**< Active member of @ref payload. */
    union {
        sensor_sample_batch_t batch;          /**< Batch awaiting averaging and output. */
        char text[SENSOR_OUTPUT_TEXT_LENGTH]; /**< Null-terminated response text. */
    } payload;                                /**< Message content selected by @ref type. */
} sensor_output_message_t;

/** @brief Queue consumed by @ref sensor_output_task. */
static os_queue_t output_queue;

/** @brief Backing storage for @ref output_queue. */
static sensor_output_message_t output_storage[SENSOR_OUTPUT_QUEUE_CAPACITY];

/** @brief UART used for both sensor streaming and the interactive shell. */
extern UART_HandleTypeDef huart1;

/**
 * @brief Transmit one complete UART fragment.
 * @param text Bytes to transmit.
 * @param length Number of bytes in @p text.
 * @return @c true when the length fits the HAL API and transmission succeeds.
 */
static bool uart_write(const char *text, size_t length) {
    return (length <= UINT16_MAX) &&
           (HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)length, HAL_MAX_DELAY) == HAL_OK);
}

/**
 * @brief Convert a floating-point value to a rounded fixed-point milli-unit.
 * @param value Value to scale.
 * @return Rounded value multiplied by 1000.
 */
static int32_t to_milli(float value) {
    float scaled = value * 1000.0f;

    return (int32_t)(scaled + ((scaled >= 0.0f) ? 0.5f : -0.5f));
}

/**
 * @brief Format a signed milli-unit value with three fractional digits.
 * @param[out] output Destination buffer.
 * @param output_size Size of @p output in bytes.
 * @param milli Fixed-point value to format.
 */
static void format_milli(char *output, size_t output_size, int32_t milli) {
    uint32_t magnitude;

    if (milli < 0) {
        magnitude = (uint32_t)(-(int64_t)milli);
        (void)snprintf(output,
                       output_size,
                       "-%lu.%03lu",
                       (unsigned long)(magnitude / 1000u),
                       (unsigned long)(magnitude % 1000u));
    } else {
        magnitude = (uint32_t)milli;
        (void)snprintf(output,
                       output_size,
                       "%lu.%03lu",
                       (unsigned long)(magnitude / 1000u),
                       (unsigned long)(magnitude % 1000u));
    }
}

/**
 * @brief Average and transmit one batch as a @c DATA CSV record.
 * @param batch Batch to serialize.
 * @return @c true when disabled/empty or when formatting and transmission succeed.
 */
static bool uart_send_batch(const sensor_sample_batch_t *batch) {
    static char values[6][20];
    static char line[160];

    if ((batch->count == 0u) || !sensor_shell_stream_enabled()) {
        return true;
    }

    for (uint32_t axis = 0u; axis < 3u; ++axis) {
        format_milli(values[axis],
                     sizeof(values[axis]),
                     to_milli(batch->sum.acceleration_g[axis] / (float)batch->count));
        format_milli(values[axis + 3u],
                     sizeof(values[axis + 3u]),
                     to_milli(batch->sum.angular_rate_dps[axis] / (float)batch->count));
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
                          (unsigned long)batch->count);

    if ((length <= 0) || (length >= (int)sizeof(line))) {
        return false;
    }

    trace_transmission_complete();
    return uart_write(line, (size_t)length);
}

/**
 * @brief Transmit one bounded, null-terminated text message.
 * @param text Text stored in an output queue entry.
 * @return @c true when the string is terminated and transmission succeeds.
 */
static bool uart_send_text(const char *text) {
    size_t length = 0u;

    while ((length < SENSOR_OUTPUT_TEXT_LENGTH) && (text[length] != '\0')) {
        ++length;
    }

    return (length < SENSOR_OUTPUT_TEXT_LENGTH) && uart_write(text, length);
}

os_status_t sensor_output_init(void) {
    return os_queue_init(&output_queue,
                         SENSOR_OUTPUT_QUEUE_ID,
                         output_storage,
                         sizeof(output_storage[0]),
                         SENSOR_OUTPUT_QUEUE_CAPACITY);
}

os_status_t sensor_output_post_batch(const sensor_sample_batch_t *batch) {
    if (batch == 0) {
        return OS_ERR_NULL;
    }

    sensor_output_message_t message = { .type = SENSOR_OUTPUT_BATCH };
    message.payload.batch = *batch;

    return os_queue_send(&output_queue, &message, OS_NO_WAIT);
}

os_status_t sensor_output_post_text(const char *format, ...) {
    if (format == 0) {
        return OS_ERR_NULL;
    }

    sensor_output_message_t message = { .type = SENSOR_OUTPUT_TEXT };
    va_list arguments;

    va_start(arguments, format);
    int length = vsnprintf(message.payload.text, sizeof(message.payload.text), format, arguments);
    va_end(arguments);

    if ((length < 0) || (length >= (int)sizeof(message.payload.text))) {
        return OS_ERR_INVALID_ARG;
    }

    return os_queue_send(&output_queue, &message, SENSOR_OUTPUT_POST_TIMEOUT);
}

void sensor_output_task(void) {
    static const char header[] = "TYPE,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,samples\r\n";
    sensor_output_message_t message;

    if (!uart_write(header, sizeof(header) - 1u)) {
        sensor_app_record_error();
    }

    shell_init();

    while (1) {
        shell_update();

        os_status_t status = os_queue_recv(&output_queue, &message, SENSOR_SHELL_POLL_TICKS);

        if (status == OS_ERR_TIMEOUT) {
            continue;
        }

        if (status != OS_OK) {
            sensor_app_record_error();
            continue;
        }

        do {
            bool succeeded =
                (message.type == SENSOR_OUTPUT_BATCH)  ? uart_send_batch(&message.payload.batch)
                : (message.type == SENSOR_OUTPUT_TEXT) ? uart_send_text(message.payload.text)
                                                       : false;

            if (!succeeded) {
                sensor_app_record_error();
            }

            status = os_queue_recv(&output_queue, &message, OS_NO_WAIT);
        } while (status == OS_OK);

        if (status != OS_ERR_WOULD_BLOCK) {
            sensor_app_record_error();
        }
    }
}

#endif /* PROJECT == PROJECT_SENSOR */
