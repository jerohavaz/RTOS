#ifndef INTEGRATION_SENSOR_DEVICE_H_
#define INTEGRATION_SENSOR_DEVICE_H_

#include <stdbool.h>

#include "integration_sensor_types.h"

bool sensor_device_init(void);
bool sensor_device_set_mode(sensor_mode_t mode);
bool sensor_device_read(sensor_sample_t *sample);
bool sensor_device_read_status(sensor_device_status_t *status);
bool sensor_device_reset(void);

#endif /* INTEGRATION_SENSOR_DEVICE_H_ */
