
# Blocking Delay TeSSLa Verification

This monitor verifies the blocking delay (`os_delay_busy`) requirements using TeSSLa. Verification is based solely on the emitted delay and task events, ensuring that active busy-waiting delays maintain the correct execution state and duration constraints.

## Verified Properties

* **Blocking delays must never block:** Blocking delays (`os_delay_busy`) must not trigger a task state transition to `BLOCKED`.
* **Execution state retention:** A task invoking a busy delay must be in the `RUNNING` state at the moment the delay starts.
* **Minimum duration respected:** A task must not complete its busy delay before the requested number of system ticks has elapsed.

## Test Suite

The blocking delay test suite provides dedicated traces for:

* Valid busy delay execution
* Invalid state transition to `BLOCKED` during a busy delay
* Premature busy delay completion before time expiration

## State Encoding

```text
0 = CREATED
1 = READY
2 = RUNNING
3 = BLOCKED
```