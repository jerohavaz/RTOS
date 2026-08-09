# Delay TeSSLa Verification

This monitor verifies both non-blocking (`os_delay`) and blocking (`os_delay_busy`) delay requirements using TeSSLa. Verification is based on emitted delay, task state, and tick events to ensure correct timing and execution state behavior.

## Verified Properties

### Non-Blocking Delay (`os_delay`)
* **State Transition to Blocked:** Calling a non-zero delay must immediately transition the task from `RUNNING` to `BLOCKED`.
* **Timeout List Registration:** A non-blocking delay must register a timeout event with the specified tick duration.
* **Minimum Duration Respected:** A task must not be unblocked before the requested delay ticks have elapsed.
* **Yield on Zero Delay:** Calling `os_delay(0)` must request a scheduler yield without blocking or adding a timeout.

### Blocking Delay (`os_delay_busy`)
* **Never Blocks:** Active delays must never trigger a task state transition to `BLOCKED`.
* **Execution State Retention:** A task invoking a busy delay must be in the `RUNNING` state when the delay starts.
* **Minimum Duration Respected:** A task must not complete its busy delay before the requested number of system ticks has elapsed.

## Test Suite

The delay test suite provides dedicated traces for:

* Valid non-blocking delay execution (`os_delay`)
* Valid blocking delay execution (`os_delay_busy`)
* Premature unblock violations for `os_delay`
* Invalid state transitions to `BLOCKED` during `os_delay_busy`
* Premature completion violations before time expiration for `os_delay_busy`
* Zero-tick yield handling (`os_delay(0)`)

## State Encoding

```text
0 = CREATED
1 = READY
2 = RUNNING
3 = BLOCKED