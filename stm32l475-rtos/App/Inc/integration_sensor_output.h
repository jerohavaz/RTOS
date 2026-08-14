/**
 * @file integration_sensor_output.h
 * @brief Serialized UART output for sensor data and shell responses.
 * @author Jerome
 * @author Martin
 */

#ifndef INTEGRATION_SENSOR_OUTPUT_H_
#define INTEGRATION_SENSOR_OUTPUT_H_

#include "integration_sensor_types.h"
#include "os_types.h"

/**
 * @brief Initialize the sensor output queue.
 * @return @c OS_OK on success, otherwise an RTOS queue error.
 */
os_status_t sensor_output_init(void);

/**
 * @brief Queue one completed sample batch without blocking.
 * @param batch Batch copied into the output queue.
 * @return @c OS_OK on success or an RTOS queue error.
 */
os_status_t sensor_output_post_batch(const sensor_sample_batch_t *batch);

/**
 * @brief Format and queue a shell response or error message.
 * @param format @c printf-compatible format string.
 * @param ... Values referenced by @p format.
 * @return @c OS_OK on success, @c OS_ERR_INVALID_ARG if the formatted text is
 *         too long, or another RTOS queue error.
 */
os_status_t sensor_output_post_text(const char *format, ...);

/**
 * @brief Run the UART output and shell worker task.
 *
 * The task owns normal UART transmission, polls the shell, and drains queued
 * batches and text messages. It never returns.
 */
void sensor_output_task(void);

#endif /* INTEGRATION_SENSOR_OUTPUT_H_ */
