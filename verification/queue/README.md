# Message Queue TeSSLa Verification

This monitor verifies the project’s message queue requirements from emitted queue, scheduler and task events. It generates independent state for every configured queue.

## Verified Properties

* Fill remains between `0` and the configured capacity.
* Buffered reads from an empty queue and writes to a full queue cannot succeed.
* Buffered messages are received unchanged and in FIFO order.
* Direct handoffs preserve the message and do not change the fill level.
* Blocking operations transition the task from `RUNNING` to `BLOCKED` and emit `BLOCKED`.
* Woken tasks transition from `BLOCKED` to `READY` and emit `READY`.
* A waiting sender is awakened after a slot becomes free.
* A waiting receiver is awakened after a direct handoff.
* Finite timeouts do not occur early.
* `OS_NO_WAIT` never blocks or times out; `OS_WAIT_FOREVER` never times out.

Timeout duration is calculated from `TICK dt`, not TeSSLa input timestamps.

## Test Suite

The tests cover valid buffered FIFO operation, direct handoff and receive timeout, plus isolated violations of capacity, FIFO order, message integrity, empty/full access, block/wake transitions, timeout timing and required wake events.

`max_tasks` is the task count, allowing IDs `0..max_tasks - 1`. Queue IDs may be non-contiguous and may have different capacities.

## Trace Requirements

`QUEUE_FILL` must follow every ring-buffer push or pop. `QUEUE_HANDOFF` must precede its send/receive success pair and must not change fill. Block and wake events must be followed immediately by their matching task-state and scheduler events.

ISR operations use task ID `255`. They may complete immediately but must not block or time out.

## Limitations

* Integrity uses 32-bit hashes, so collisions are possible.
* Complete, ordered RTT events are required.
* Missing block/wake events are reported on the next `TICK`; an earlier trace end hides the violation.
* Only configured queues and task IDs are monitored.
* Timeout checks reject early completion but allow late completion.
* Wait-queue priority ordering is not verified.