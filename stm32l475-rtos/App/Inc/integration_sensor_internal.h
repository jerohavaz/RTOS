/**
 * @file integration_sensor_internal.h
 * @brief Shared internal sensor-application services.
 * @author Jerome
 * @author Martin
 */

#ifndef INTEGRATION_SENSOR_INTERNAL_H_
#define INTEGRATION_SENSOR_INTERNAL_H_

/**
 * @brief Record a runtime sensor-application error and light the error LED.
 *
 * This function may be called from the sensor tasks or an interrupt callback.
 */
void sensor_app_record_error(void);

#endif /* INTEGRATION_SENSOR_INTERNAL_H_ */
