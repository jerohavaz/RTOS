#ifndef INTEGRATION_SENSOR_TYPES_H_
#define INTEGRATION_SENSOR_TYPES_H_

#include <stdint.h>

typedef struct {
    float acceleration_g[3];
    float angular_rate_dps[3];
} sensor_sample_t;

typedef struct {
    sensor_sample_t sum;
    uint32_t count;
} sensor_sample_batch_t;

typedef enum { SENSOR_MODE_LOW = 0, SENSOR_MODE_NORMAL, SENSOR_MODE_HIGH } sensor_mode_t;

typedef struct {
    uint8_t ctrl1_xl;
    uint8_t ctrl2_g;
} sensor_device_status_t;

#endif /* INTEGRATION_SENSOR_TYPES_H_ */
