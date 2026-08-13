#ifndef INTEGRATION_SENSOR_OUTPUT_H_
#define INTEGRATION_SENSOR_OUTPUT_H_

#include "integration_sensor_types.h"
#include "os_types.h"

os_status_t sensor_output_init(void);
os_status_t sensor_output_post_batch(const sensor_sample_batch_t *batch);
os_status_t sensor_output_post_text(const char *format, ...);
void sensor_output_task(void);

#endif /* INTEGRATION_SENSOR_OUTPUT_H_ */
