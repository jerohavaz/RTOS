/**
 * @file integration_sensor_shell.h
 * @brief Interrupt-driven UART shell for the sensor application.
 * @author Jerome
 * @author Martin
 */

#ifndef INTEGRATION_SENSOR_SHELL_H_
#define INTEGRATION_SENSOR_SHELL_H_

#include <stdint.h>

/** @brief Reset shell state, print the prompt, and start interrupt reception. */
void shell_init(void);

/** @brief Parse and dispatch one completed input line when available. */
void shell_update(void);

/**
 * @brief Query whether periodic sensor records are enabled.
 * @return Nonzero while streaming is enabled, otherwise zero.
 */
uint8_t sensor_shell_stream_enabled(void);

#endif /* INTEGRATION_SENSOR_SHELL_H_ */
