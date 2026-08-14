# Message Queue Verification

Verifies configured queues from queue and task events.

## Checks

- Observed fill and the internal FIFO model remain within `0..capacity`.
- Each buffered send or receive has the expected immediately preceding `QUEUE_FILL` value; direct handoff preserves fill.
- Buffered reads from empty queues and writes to full queues cannot succeed.
- Buffered messages preserve their 32-bit hash and FIFO order.
- Direct-handoff success events match the sender, receiver, and message hash.
- Blocking and waking are followed by the matching state transition and scheduler event.
- A successful send while a receiver is still waiting requires direct handoff and receiver wakeup.
- Freeing a buffered slot while a sender waits requires sender completion and wakeup.
- Finite timeout events cannot occur early; `OS_NO_WAIT` cannot block or time out, and `OS_WAIT_FOREVER` cannot time out.

## Implementation Trace

- Sending to a waiting receiver emits handoff, send success, receive success, receiver wake, and its ready sequence.
- A buffered receive may refill the freed slot from a waiting sender before emitting sender success and wake.
- Buffered push and pop emit `QUEUE_FILL` before success. Message hashes are 32-bit FNV-1a.

## Limits

- The monitor does not require a finite timeout event eventually to occur.
- Final missing block, ready, or wake obligations need a later `TICK` to be classified.
- Receiver handoff is checked at send success, after the queue operation's outcome is known. A receiver may time out or a tick may preempt the sender after send attempt without constituting a violation.
- Hash collisions are possible, so message equality is probabilistic rather than byte-exact.
- Only configured queue IDs, capacities, and task IDs are modeled; waiter priority order is not checked.
