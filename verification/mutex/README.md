# Mutex TeSSLa Verification

This module verifies observed executions of the non-recursive `os_mutex_t`
implementation. Mutex addresses are assigned dynamically to a bounded number
of monitor slots, allowing operations on several mutex instances to be
interleaved in one trace.

## Verified Properties

### Ownership

- A newly initialized mutex is unlocked.
- Every ownership snapshot agrees with the previously observed owner.
- A successful immediate lock of an unlocked mutex makes the caller its sole
  owner.
- A successful direct handoff makes exactly the selected waiter the new owner.
- An unsuccessful lock or unlock cannot change ownership.
- Only the current owner can successfully unlock the mutex.
- A mutex owner cannot successfully lock the same non-recursive mutex again.
- A task cannot immediately acquire a mutex that is already owned by another
  task.
- Reinitializing an owned mutex or a mutex with queued waiters is rejected by
  the trace model.

### Lock, blocking and timeout

- Lock entry and exit events form one non-overlapping operation per task.
- A block event belongs to an active lock of the same mutex and preserves its
  owner and timeout parameters.
- The first task-state transition following `MUTEX_BLOCK` must move the same
  task into `BLOCKED`.
- A finite wait cannot time out before its requested number of emitted kernel
  ticks has elapsed.
- A timed-out lock must later report failure.
- A waiter selected by `MUTEX_WAKE` must later report successful acquisition.

### Unlock and waiter selection

- A successful unlock with no waiter changes the owner to “none”.
- A successful unlock with waiters performs direct ownership handoff.
- Such an unlock must be followed by `MUTEX_WAKE` for the same mutex and the
  owner chosen by the unlock.
- A wake must target a task currently queued on that mutex.
- Higher numeric task priority is selected first.
- Equal-priority waiters are selected FIFO according to `MUTEX_BLOCK` order.
- Timeout and wake events cannot resolve a nonexistent or already-resolved
  wait.

## Violation Streams

| Stream | Meaning |
| --- | --- |
| `violation_mutex_invalid_create` | A mutex is reinitialized while owned or while tasks are waiting. |
| `violation_mutex_owner_discontinuity` | A traced owner snapshot or ownership transition contradicts the monitored owner. |
| `violation_mutex_invalid_unlock` | The unlock actor, result, or resulting owner is invalid. |
| `violation_mutex_recursive_lock` | A non-recursive lock succeeds for its existing owner. |
| `violation_mutex_locked_acquire_succeeded` | An owned mutex is acquired immediately by another task. |
| `violation_mutex_invalid_lock` | Lock events overlap, mismatch, or resolve inconsistently. |
| `violation_mutex_blocking_state` | A blocked lock is not confirmed by the required task-state transition. |
| `violation_mutex_timeout_too_early` | A finite wait times out before its requested tick duration. |
| `violation_mutex_timeout_result` | A timed-out lock later reports success. |
| `violation_mutex_missing_wake` | A successful unlock with waiters is not followed by the required wake. |
| `violation_mutex_wake_priority` | A lower-priority waiter is selected while a higher-priority waiter exists. |
| `violation_mutex_wake_fifo` | Equal-priority waiter FIFO order is violated. |
| `violation_mutex_invalid_wait_lifecycle` | A timeout or wake does not resolve a matching queued waiter. |
| `violation_mutex_untracked_mutex` | An event has no assigned slot or the configured mutex bound is exceeded. |

## Test Suite

Valid fixtures cover:

- immediate lock and unlock;
- rejected recursive lock, non-owner unlock and non-blocking contention;
- blocking acquisition with direct handoff;
- finite timeout;
- priority ordering and equal-priority FIFO ordering;
- two interleaved mutex objects.

Invalid fixtures cover every violation stream, including inconsistent owner
snapshots, successful non-owner unlock, recursive acquisition, acquisition of
an already-owned mutex, malformed lock lifecycle, missing `BLOCKED`, early and
successful timeout, missing and spurious wake, wrong priority/FIFO selection,
and monitor-bound overflow.

Run the fixtures with:

```bash
python3 verify.py test mutex --tessla-jar /path/to/tessla.jar
```

## Verification Boundaries

This is runtime verification of the recorded execution, not a proof of all
possible executions.

- The monitor is bounded by `max_tasks` and `max_mutexes`. Out-of-bound mutex
  observations produce an explicit violation.
- Mutex identity is the traced object address. The address must remain stable
  for the object's lifetime.
- Correct conclusions require a complete, ordered trace. Run the `integrity`
  module alongside this module; a sequence gap makes ownership and wake
  conclusions potentially inconclusive.
- Trace fields and their critical-section ordering are trusted instrumentation
  inputs. State changes omitted from the trace cannot be reconstructed.
- Timeout duration is based on emitted `TICK` values. The monitor proves only
  that timeout is not early; scheduling or critical sections may legitimately
  delay processing.
- The required-wake check is future-independent: a missing wake is reported
  when the next mutex event is not `MUTEX_WAKE`. If recording ends immediately
  after an unlock, the pending obligation cannot yet be classified.
- Null handles, invalid timeout arguments, ISR calls and other API failures
  rejected before mutex tracing begins are outside this monitor.
- The implementation has no priority inheritance. The monitor checks waiter
  selection order, not owner-priority boosting or priority inversion bounds.
- Task ID `255` represents “no owner” or “no task”; it is never a valid waiter.
