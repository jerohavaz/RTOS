# Quick Start Guide

## 1. STM32CubeIDE Setup

Import `stm32l475-rtos` as an **STM32 CMake Project**, select the **STM32L475VGTx**, and use the **SEGGER J-Link Debugger**.

Select the application or integration test in `stm32l475-rtos/App/Inc/project.h`, for example:

```c
#define PROJECT PROJECT_SENSOR
```

For the complete setup, see [STM32CubeIDE Setup](SETUP.md#stm32cubeide).

## 2. SEGGER SystemView

Open the provided project:

```text
stm32l475-rtos/RTOS.SVPrj
```

Then:

1. Start the firmware/debug session.
2. Open SEGGER SystemView.
3. Select **Target → Start Recording**.

The trace shows task states, idle time, interrupts, delays, mutexes, semaphores, and queues.

For more information, see [SEGGER SystemView Setup](stm32l475-rtos/SEGGER.md).

## 3. LSM6DSL Sensor Project

`PROJECT_SENSOR` reads the accelerometer and gyroscope and sends averaged sensor values over UART every 100 ms.

Example output:

```text
DATA,0.012,-0.004,0.998,1.230,-0.420,0.115,10
```

Example shell commands:

```text
status
mode low
mode normal
mode high
reset
```

Manually read data and enter commands:

```bash
screen /dev/ttyACM0 115200
```

For the architecture, UART format, and all available commands, see [LSM6DSL Sensor Application](stm32l475-rtos/App/SENSOR.md).

Additional tools for the sensor project:

* [Sensor Terminal](sensor_terminal/README.md) — interactive UART terminal for sensor data and shell commands.
* [Sensor Viewer](sensor_viewer/README.md) — graphical visualization of accelerometer and gyroscope data.

## 4. TeSSLa RTT Trace

For a complete trace, start the debugger and stop at `HAL_Init()` before starting the RTT collector.

Record a trace:

```bash
cd verification
python3 rtt_to_tessla.py -o trace.input
```

Then resume the target.

For live verification with a compiled monitor:

```bash
python3 rtt_to_tessla.py --stdout | build/combined-monitor
```

For more information, see [TeSSLa RTT Verification](verification/README.md#rtt-into-the-tessla-interpreter).

## 5. Prebuilt Integration-Test Monitors

> **[Download the integration-test monitors (`monitors.zip`)](https://drive.google.com/file/d/1FiHaoeGVnvxhUGxd2dAXaXvNWMbt5crq/view?usp=sharing)**

The archive contains prebuilt monitors and generated TeSSLa specifications for all integration-test configurations.

Example using a recorded queue trace:

```bash
./monitors/violations/queue-monitor < trace.input
```

For the sensor project:

```bash
./monitors/violations/sensor-monitor < trace.input
```

Both `checks/` and `violations/` monitor variants are included.

For the exact monitor configuration of each integration test, see [Prebuilt TeSSLa Monitors](stm32l475-rtos/App/README.md#prebuilt-tessla-monitors).
