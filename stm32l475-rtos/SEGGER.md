# SEGGER SystemView

Use the provided SystemView project file:

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

Task `0` is the RTOS idle task. It is intentionally **not** registered as a normal SystemView task. Idle execution is represented exclusively with SEGGER SystemView's `SEGGER_SYSVIEW_OnIdle()` event, so idle time appears in SystemView's dedicated idle representation instead of the normal task list/timeline.

Normal task metadata is cached by the RTOS trace subsystem when each task is created. SystemView's `pfSendTaskList` callback replays that trace-owned metadata whenever SystemView requests the current task list. Therefore recording may start after task creation without requiring SystemView or the trace layer to query the kernel task table.

`trace_init()` remains before task creation. New normal tasks are announced immediately with `SEGGER_SYSVIEW_OnTaskCreate()` and `SEGGER_SYSVIEW_SendTaskInfo()`, while a later recording restart can recover the current normal-task metadata through `pfSendTaskList`.