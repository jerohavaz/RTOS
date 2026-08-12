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

## Idle task representation

Task `0` is the RTOS idle task. It is created and registered with SystemView like the other tasks, so it appears in the task list and its creation event is visible.

Its execution is intentionally not reported with the normal task-execution event used for application tasks. Instead, idle execution is reported using SEGGER SystemView's `SEGGER_SYSVIEW_OnIdle()` event.

As a result, task `0` may appear to have been created but never run in the normal task timeline. Idle periods are shown by SystemView's dedicated idle representation instead. This is expected behavior and does not mean that the idle task is not executing.