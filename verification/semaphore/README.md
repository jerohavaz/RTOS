# Counting-Semaphore TeSSLa Verification

This module verifies observed executions of `os_sem_t`. It supports counting
semaphores directly; a binary semaphore is the special case `max_count == 1`.
Semaphore addresses are assigned dynamically to a bounded number of monitor
slots, so operations on several semaphore instances may be interleaved.

## Verified Properties

### Count and capacity

* A created semaphore has `max_count > 0` and
  `0 <= initial_count <= max_count`.
* Every observed count is within the configured capacity.
* Count snapshots are continuous across create, acquire, release and timeout
  events for each tracked semaphore.
* An immediate successful acquire decrements the count by exactly one.
* An unsuccessful immediate acquire leaves the count unchanged.
* A release without a waiter increments the count by exactly one.
* A release at full capacity fails and leaves the count unchanged.
* Direct handoff to a waiter succeeds without incrementing the stored count.

### Acquire, blocking and timeout

* Acquire enter/exit events form one non-overlapping operation per task.
* An immediate acquire observed with count zero cannot report success.
* A blocking acquire is preceded by an empty acquire and preserves the
  requested timeout parameters.
* The first task-state transition after `SEM_BLOCK` must move the same task to
  `BLOCKED`.
* A timed wait cannot emit `SEM_TIMEOUT` before its requested number of kernel
  ticks has elapsed.
* A timed-out acquire must later report failure; an acquire selected by
  `SEM_WAKE` must later report success.

### Release and waiter selection

* A successful release with queued waiters uses direct handoff.
* Such a release must be followed by `SEM_WAKE` for the same semaphore.
* A wake must select a task currently queued on that semaphore.
* Higher numeric task priority is selected first.
* Equal-priority waiters are selected FIFO according to `SEM_BLOCK` order.
* Timeout and wake operations remove only existing waiters and cannot resolve
  the same wait twice.

## Violation Streams

| Stream | Meaning |
| --- | --- |
| `violation_sem_invalid_create` | Invalid initial count/capacity or reinitialization with waiters. |
| `violation_sem_count_out_of_range` | An observed count is outside `[0, max_count]`. |
| `violation_sem_count_discontinuity` | A count snapshot does not follow the previously monitored value. |
| `violation_sem_invalid_release` | Release result or count delta contradicts capacity/waiter state. |
| `violation_sem_empty_acquire_succeeded` | An immediate empty acquire reports success. |
| `violation_sem_invalid_acquire` | Acquire events overlap, mismatch, or report an invalid result. |
| `violation_sem_blocking_state` | A blocking acquire is not confirmed as `BLOCKED`. |
| `violation_sem_timeout_too_early` | Timeout occurs before the requested tick duration. |
| `violation_sem_timeout_result` | A timed-out acquire later reports success. |
| `violation_sem_missing_wake` | A release with waiters is not followed by a wake. |
| `violation_sem_wake_priority` | A lower-priority waiter is selected first. |
| `violation_sem_wake_fifo` | Equal-priority FIFO order is violated. |
| `violation_sem_invalid_wait_lifecycle` | Timeout/wake targets no matching queued waiter or lacks its release. |
| `violation_sem_untracked_semaphore` | An operation has no slot or the configured instance bound is exceeded. |

## Test Suite

The fixtures include valid traces for:

* counting and binary semaphore operations;
* blocking release with direct handoff;
* finite timeout completion;
* higher-priority-first and equal-priority FIFO selection;
* two interleaved semaphore instances.

Invalid fixtures cover every violation stream, including full-release success,
empty-acquire success, early timeout, successful timeout result, missing and
spurious wakeups, wrong priority/FIFO selection, inconsistent count snapshots,
and monitor-bound overflow.

## Verification Boundaries

This is runtime verification of the recorded execution, not a proof of every
possible execution.

* The generated monitor is bounded by `max_tasks` and `max_semaphores`.
  Out-of-bound observations produce explicit violations.
* Semaphore identity is the traced object address. The address must stay stable
  for the object's lifetime. A new `SEM_CREATE` resets the monitored count and
  capacity for a reused known address.
* Correct conclusions require a complete, ordered trace. Run the `integrity`
  module together with this module. A sequence gap makes the affected result
  inconclusive because a missing release, wake or timeout can change the model.
* Trace fields and their critical-section ordering are trusted instrumentation
  inputs. The monitor detects inconsistent snapshots but cannot reconstruct a
  state change that was never traced.
* Timeout duration is measured in emitted kernel `TICK` values. The monitor
  proves that a timeout is not early; it intentionally sets no maximum latency,
  because critical sections and scheduling may delay timeout processing.
* TeSSLa is future-independent. A required wake is reported missing when the
  next semaphore event is not `SEM_WAKE`. If recording stops immediately after
  the release, that final pending obligation cannot yet be classified. The same
  finite-prefix limitation applies to a blocked acquire whose exit has not yet
  appeared and may still be legitimately pending.
* Invalid API calls rejected before semaphore tracing starts—such as null
  handles, an invalid timeout argument, or a failed `os_sem_init`—are not
  observable and therefore are outside this module.
* Task ID `255` represents an acquire with no owning task, such as an
  exception-context or pre-scheduler operation. It may complete immediately but
  cannot enter the semaphore wait queue.
