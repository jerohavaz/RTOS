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
