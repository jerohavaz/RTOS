#ifndef INTEGRATION_SENSOR_SHELL_H_
#define INTEGRATION_SENSOR_SHELL_H_

#include <stdint.h>

void shell_init(void);
void shell_update(void);
uint8_t sensor_shell_stream_enabled(void);

#endif /* INTEGRATION_SENSOR_SHELL_H_ */
