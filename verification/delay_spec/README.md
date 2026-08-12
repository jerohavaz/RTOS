# Delay Verification

Verifies observed `os_delay` and `os_delay_busy` executions.

## Checks

- A nonzero `os_delay` must be followed by `RUNNING -> BLOCKED` and the matching `BLOCKED` event.
- `DELAY_END` cannot occur before the requested ticks and must be followed by `BLOCKED -> READY` and the matching `READY` event.
- `os_delay_busy` must start while the task is `RUNNING`.
- A busy delay cannot enter `BLOCKED` or finish before the requested ticks.

## Implementation Trace

- A nonzero `os_delay` emits `DELAY_START`, its block transition, and `BLOCKED`; timeout cleanup emits `DELAY_END`, its ready transition, and `READY`.
- `os_delay_busy` polls the kernel tick without blocking and remains preemptible.
- `os_delay(0)` requests a scheduler yield and emits no delay event.

## Limits

- A final missing block or ready sequence cannot be classified without a later delay, state, scheduler, or tick event.
- Busy-delay preemption and transitions through `READY` are allowed; only entry into `BLOCKED` is rejected.
- Durations use emitted kernel ticks, and task IDs are bounded by `max_tasks`.
