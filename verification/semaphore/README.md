# Semaphore Verification

Verifies observed counting `os_sem_t` instances, including binary semaphores where `max_count == 1`.

## Checks

- Creation requires `max_count > 0` and `0 <= initial_count <= max_count`; all later count snapshots remain within that capacity.
- Count changes must match successful immediate acquire, release, timeout, or direct handoff behavior.
- An immediate acquire at count zero and a release at full capacity cannot succeed.
- A blocking acquire must belong to an active operation; its first observed task-state transition must enter `BLOCKED`.
- Finite timeout events cannot occur early and must make the acquire operation fail.
- Successful release with waiters performs direct handoff without incrementing the stored count and requires the next semaphore event to wake a waiter.
- A wake must select a queued waiter by higher numeric priority, then FIFO block order among equal priorities.
- Operation lifecycles, waiter resolution, and the configured monitor bound are checked.

## Implementation Trace

- Blocking emits `SEM_BLOCK`, then `STATE ... BLOCKED` and `BLOCKED`.
- Release with waiters transfers the token directly and emits `SEM_RELEASE`, `SEM_WAKE`, `STATE ... READY`, and `READY` without incrementing the stored count.
- Timeout cleanup removes the waiter and emits `SEM_TIMEOUT`; the common timeout handler then readies the task, and `SEM_ACQUIRE_EXIT` is emitted when the task resumes.
- The implementation wait queue selects the highest numeric priority and is FIFO within one priority.

## Limits

- The monitor does not require a finite timeout event eventually to occur.
- A final pending block, acquire, or required wake cannot be classified without a later relevant event.
- Semaphore identity is its traced address; at most `max_semaphores` instances and `max_tasks` task IDs are tracked.
- Timeout checks use emitted kernel ticks and impose no maximum completion latency.
