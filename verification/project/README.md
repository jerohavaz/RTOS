# 3D-Gyro-Accelerometer TeSSLa Verification

This monitor verifies the end-to-end transmission latency requirement for the 3D-Gyro-Accelerometer project using TeSSLa. Verification is based on tracing the duration between reading sensor data and successfully transmitting it over UART.

## Verified Properties

* **Sensor Latency**: The elapsed time between a `sensor_read` event and the corresponding `transmission_complete` event must not exceed the configured limit of 100 ticks (`max_latency_ticks = 100`).

## Input Events

* `sensor_read`: Triggered when sensor data collection begins.
* `transmission_complete`: Triggered when data transmission over UART finishes.
* `tick`: Advances the global tick counter by the specified integer delta.

## Test Suite

The project test suite provides traces for:

* `valid_project_transmission.input`: Successful transmission within the 100-tick deadline.
* `bad_project_timeout.input`: Transmission deadline exceeded, triggering `violation_sensor_timeout`.

## Configuration

* `max_latency_ticks`: `100`
* `violation_sensor_timeout`: Emitted when `elapsed_ticks > max_latency_ticks`.