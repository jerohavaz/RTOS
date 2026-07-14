# Scheduler / Task TeSSLa Verification

This monitor verifies the scheduler and task requirements defined in the project specification using TeSSLa. The provided traces cover both valid execution and dedicated violation scenarios for every implemented verification property.

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
* Only valid task state transitions are permitted.
* Running task identifiers must remain within the configured task range.

The current implementation represents each task by a single state value (`CREATED`, `READY`, `RUNNING` or `BLOCKED`). Therefore, the requirements *"A task may only be in one state"* and *"A task must not be READY and BLOCKED simultaneously"* are guaranteed by the state representation itself.

The ISR requirement is not verified explicitly. Since interrupt handlers always return to the interrupted execution context and all scheduling decisions are validated independently (priority scheduling, Round-Robin, quantum handling and state transitions), this property is implicitly covered by the scheduler verification.

## Test Suite

The scheduler test suite contains dedicated traces for:

* Valid scheduler execution
* Valid Round-Robin scheduling
* Invalid state transitions
* Invalid running task identifiers
* Priority scheduling violations
* Round-Robin / quantum violations
* Idle execution while READY tasks exist
* BLOCKED task execution
* READY event inconsistencies
* RUNNING event inconsistencies
* BLOCKED event inconsistencies

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
