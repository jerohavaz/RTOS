# Integration-test application

The application builds exactly one RTOS integration test. Select it by changing
`PROJECT` in `Inc/project.h`:

```c
#define PROJECT PROJECT_QUEUE
```

| Selection | Coverage |
| --- | --- |
| `PROJECT_SCHEDULER` | Strict priority order, several tick-driven round-robin handshakes between two CPU-bound READY peers, then lower-priority progress |
| `PROJECT_DELAY` | One measured busy delay and one scheduler-aware delay, including observer exclusion/progress |
| `PROJECT_SEMAPHORE` | Binary empty/full/success paths, low-first contention with high-priority wake-up, finite timeout, and counting range |
| `PROJECT_MUTEX` | Low-first contention with high-priority handoff, one-owner invariant, recursive/non-owner rejection, finite timeout, and reuse |
| `PROJECT_QUEUE` | Empty receive and full send blocking, direct handoff, FIFO refill, exact integrity, no-wait rejection, and both finite timeout paths |

Each test has its own source file under `Src/`. Tests are intentionally small
and use only public RTOS APIs.

## Result

Inspect `g_integration_test_result` in the debugger:

```text
state     INTEGRATION_TEST_PASSED or INTEGRATION_TEST_FAILED
checks    number of evaluated conditions
failures  number of failed conditions
```

Component-specific observations are also available:

```text
g_scheduler_test_observation
g_delay_test_observation
g_semaphore_test_observation
g_mutex_test_observation
g_queue_test_observation
```

The selected scenario marks itself passed after all required interactions have
completed. Tasks then remain blocked or repeat the tested cycle; they never
return from their entry functions.

Tracing may remain enabled. SystemView and TeSSLa events provide the detailed
scheduling and object-operation sequence while the volatile result gives a
simple debugger-visible verdict.

## C checks and trace checks

The C assertions check API return values and data observations directly:

- scheduler startup order, four peer handshakes, and low-task progress only
  after both peers enter their blocking park;
- binary semaphore empty/full/consume behavior, low-first concurrent blocking,
  high-priority wake-up, finite acquire timeout, counting to three, overflow,
  complete token removal, and empty no-wait rejection;
- mutex owner-only unlock, non-recursive rejection, finite lock timeout,
  low-first concurrent blocking, high-priority ownership handoff, reuse, and
  `active_owners == 1` after every successful lock;
- queue direct handoff, empty blocking receive, full no-wait rejection,
  blocking send wake-up, FIFO, full comparison of all message fields, empty
  no-wait rejection, and finite send/receive timeouts;
- no observer progress during `os_delay_busy()` and observer progress during
  `os_delay()`.

The following temporal properties require TeSSLa evaluation of the existing
trace events and are not claimed by C assertions alone:

- a higher-priority READY task is always selected before a lower-priority one;
- both scheduler peers remain READY across several ticks, every one-tick slice
  rotates while the equal-priority peer is READY, and multiple rotations occur;
- idle executes only while no normal task is READY;
- task-state transitions are valid and a BLOCKED task never executes;
- `os_delay_busy()` causes no task-state transition, while `os_delay()` keeps
  the controller non-READY until the requested tick count and makes it READY
  when that interval expires.

Queue trace events expose only a 32-bit message hash. TeSSLa therefore checks
hash integrity; the C test performs the authoritative full-field comparison.
