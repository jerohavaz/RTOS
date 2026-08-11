# SEGGER SystemView

Use the provided SystemView project file at the repository root:

```text
RTOS.SVPrj
```

## Usage

1. Open **SEGGER SystemView**.
2. Open `RTOS.SVPrj` using **File → Load Project**.
3. Start the STM32 application/debug session.
4. In SystemView, select **Target → Start Recording**.

Always open the provided `.SVPrj` file before starting a recording. The project contains the SystemView configuration needed so the custom RTOS events are decoded correctly.

If SystemView is started without `RTOS.SVPrj`, custom events may appear with generic names instead of the expected queue, semaphore, mutex, and delay event names.