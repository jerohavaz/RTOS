# Scheduler / Task TeSSLa Verification

This monitor verifies the scheduler and task requirements defined in the project specification using TeSSLa. Verification is based solely on the emitted scheduler and task events and is therefore independent of the kernel object (e.g. delays, mutexes, semaphores or message queues) causing a scheduling decision.

## Verified Properties

### Scheduler

* Higher-priority tasks are always scheduled before lower-priority tasks.
* Tasks of equal priority are scheduled according to the Round-Robin algorithm.
* A task must not exceed its configured time quantum.
* The Idle task may only execute if no task is in the READY state.
* Only valid task state transitions are permitted.

### Tasks

* A BLOCKED task must never be scheduled.
* READY, RUNNING and BLOCKED events must be consistent with the monitored task state.
* Running task identifiers must remain within the configured task range.

Each task is represented by exactly one state (`CREATED`, `READY`, `RUNNING` or `BLOCKED`). Consequently, the requirements *"A task may only be in one state"* and *"A task must not be READY and BLOCKED simultaneously"* are inherently satisfied by the monitored state model.

The ISR requirement is not verified explicitly. Interrupt handlers always return to the interrupted execution context, while all subsequent scheduling decisions are verified through the scheduler properties (priority scheduling, Round-Robin, quantum handling and state transitions).

## Test Suite

The scheduler test suite provides dedicated traces for:

* Valid scheduler execution
* Valid Round-Robin scheduling
* Priority scheduling violations
* Round-Robin / quantum violations
* Idle execution while READY tasks exist
* Invalid task state transitions
* Invalid running task identifiers
* BLOCKED task execution
* READY event inconsistencies
* RUNNING event inconsistencies
* BLOCKED event inconsistencies

These properties were additionally validated during integration tests of delays, mutexes, binary semaphores and message queues, where no scheduler or task-state violations were detected.

## State Encoding

```text
0 = CREATED
1 = READY
2 = RUNNING
3 = BLOCKED
```

## Priority Convention

```text
Higher numeric value = higher priority.
```
