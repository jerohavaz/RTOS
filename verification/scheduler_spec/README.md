# Scheduler Verification

Verifies task-state history and fixed-priority scheduling with a one-tick time slice.

## Checks

- Task IDs are within `0..max_tasks - 1`.
- Every `STATE.old` equals the previously modeled state, and the transition to `STATE.new` is valid.
- `READY -> BLOCKED` is invalid; a task may enter `BLOCKED` only from `RUNNING`.
- `READY`, `RUNNING`, and `BLOCKED` events match the immediately preceding state transition.
- Event priorities match `TASK_CREATE` when creation data is present.
- A blocked task cannot run, and idle cannot run while a task is `READY`.
- The running task is the head of the highest-priority ready queue.
- Ready tasks of equal priority are selected in FIFO order.
- After a positive `TICK`, a running task with an equal-priority ready peer cannot be selected again; a scheduling decision must appear before the next tick.

## Implementation Trace

- The implementation inserts ready tasks at the back and removes the front task from the highest numeric priority.
- A context switch requeues a still-running task before selecting the next task.
- Each `STATE` event precedes its matching scheduler event.

## Limits

- A final pending tick-rotation obligation cannot be classified without a later decision or tick.
- One integer state per task makes simultaneous states unrepresentable; continuity and event consistency are checked explicitly.
- ISR events are not present in the TeSSLa RTT stream.
- The property that the previously interrupted task must execute immediately after every ISR is intentionally not verified. The Cortex-M4 supports exception tail-chaining: if another exception is pending and eligible when an exception handler completes, the processor skips restoring the interrupted context and transfers directly to the pending exception handler. In this RTOS, PendSV may therefore execute directly after another ISR without the interrupted task executing in between. Requiring an observable return to the previously running task after every ISR would therefore not match the Cortex-M4 exception model.

## Reference

- STMicroelectronics, *PM0214 — STM32 Cortex-M4 MCUs and MPUs Programming Manual*, Rev. 10, Section 2.3.7 "Exception entry and return", p. 42. The manual describes Cortex-M4 tail-chaining: when an eligible exception is pending at completion of an exception handler, the stack pop is skipped and control transfers directly to the new exception handler.