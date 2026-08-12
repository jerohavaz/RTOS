# Mutex Verification

Verifies observed executions of non-recursive `os_mutex_t` instances.

## Checks

- Each mutex has one tracked owner, and owner snapshots and transitions must remain consistent.
- Only the owner can unlock successfully.
- Recursive acquisition and immediate acquisition of a mutex owned by another task cannot succeed.
- A blocking lock must belong to an active operation; its first observed task-state transition must enter `BLOCKED`.
- Finite timeouts cannot occur early and must make the lock operation fail.
- Successful unlock with waiters performs direct ownership handoff and requires the next mutex event to wake the selected waiter.
- A wake must select a queued waiter by higher numeric priority, then FIFO block order among equal priorities.
- Initialization, operation lifecycles, waiter resolution, and the configured monitor bound are checked.

## Implementation Trace

- Blocking emits `MUTEX_BLOCK`, then `STATE ... BLOCKED` and `BLOCKED`.
- Unlock with waiters transfers ownership directly and emits `MUTEX_UNLOCK`, `MUTEX_WAKE`, `STATE ... READY`, and `READY`.
- Timeout cleanup removes the waiter and emits `MUTEX_TIMEOUT`; the common timeout handler then readies the task, and `MUTEX_LOCK_EXIT` is emitted when the task resumes.
- The implementation wait queue selects the highest numeric priority and is FIFO within one priority.

## Limits

- A final pending block, lock, or required wake cannot be classified without a later relevant event.
- Mutex identity is its traced address; at most `max_mutexes` instances and `max_tasks` task IDs are tracked.
- Timeout checks use emitted kernel ticks and impose no maximum completion latency.
- Priority inheritance is not checked.
