# Integration-test application

The application builds exactly one RTOS integration test. Select it by changing `PROJECT` in `Inc/project.h`:

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
| `PROJECT_SENSOR` | Interrupt-driven LSM6DSL sampling, 100 ms aggregation, UART streaming, and shell commands |

The sensor application is documented separately in [SENSOR.md](SENSOR.md), including its architecture, data path, UART format, operating modes, commands,and companion host tools.

The sensor project separates device register access, sampling orchestration, UART output, and shell parsing. Raw samples are accumulated by the sensor task and one batch is queued on a fixed 100 ms RTOS-tick deadline. The data-ready semaphore uses the remaining deadline as a finite timeout, so low sensor rates do not quantize the UART period to the next sample interrupt. Queue 1 carries sensor commands with capacity 8; queue 2 carries output batches and responses with capacity 96.

Each test has its own source file under `Src/`. Tests are intentionally small and use only public RTOS APIs.

## Prebuilt TeSSLa monitors

> **[Download the integration-test monitors (`monitors.zip`)](https://drive.google.com/file/d/1FiHaoeGVnvxhUGxd2dAXaXvNWMbt5crq/view?usp=sharing)**

The archive contains native Rust monitors and their generated TeSSLa specifications. They were generated specifically for the task counts, kernel objects, verification modules, and trace configuration used by the integration tests in this application. Do not assume that these bounds are suitable for another application configuration.

For more information about monitor generation, trace capture, running monitors, verification modes, and interpreting results, see the **[TeSSLa verification guide](../../verification/README.md)**.

The filename identifies the integration-test project for which the combined monitor was generated. For example, `mutex.tessla` is the combined monitor for `PROJECT_MUTEX`; it contains every module listed in the corresponding command below, not only the mutex module.

```text
monitors/
├── checks/
│   ├── delay-monitor
│   ├── delay.tessla
│   ├── mutex-monitor
│   ├── mutex.tessla
│   ├── queue-monitor
│   ├── queue.tessla
│   ├── scheduler-monitor
│   ├── scheduler.tessla
│   ├── semaphore-monitor
│   ├── semaphore.tessla
│   ├── sensor-monitor
│   └── sensor.tessla
└── violations/
    ├── delay-monitor
    ├── delay.tessla
    ├── mutex-monitor
    ├── mutex.tessla
    ├── queue-monitor
    ├── queue.tessla
    ├── scheduler-monitor
    ├── scheduler.tessla
    ├── semaphore-monitor
    ├── semaphore.tessla
    ├── sensor-monitor
    └── sensor.tessla
```

The files under `checks/` were generated with `--mode checks`. The files under `violations/` were generated with `--mode violations`. Each command below was run once for each mode by replacing `MODE` with `checks` and then `violations`. The resulting `build/combined.tessla` and `build/combined-monitor` were renamed to the project-specific filenames shown above.

All projects use the following base trace configuration in `Config/os_config.h`:

```c
#define OS_TRACE_ENABLED (true)
#define OS_TRACE_TESSLA_RTT (true)
#define OS_TRACE_SCHEDULER (true)
#define OS_TRACE_TASKS (true)
#define OS_TRACE_DELAY (true)
```

Additional trace sources are enabled per project:

| Project             | Additional `Config/os_config.h` traces                     |
| ------------------- | ---------------------------------------------------------- |
| `PROJECT_SCHEDULER` | —                                                          |
| `PROJECT_DELAY`     | —                                                          |
| `PROJECT_SEMAPHORE` | `OS_TRACE_SEMAPHORE`                                       |
| `PROJECT_MUTEX`     | `OS_TRACE_SEMAPHORE`, `OS_TRACE_MUTEX`                     |
| `PROJECT_QUEUE`     | `OS_TRACE_QUEUE`                                           |
| `PROJECT_SENSOR`    | `OS_TRACE_SEMAPHORE`, `OS_TRACE_QUEUE`, `OS_TRACE_PROJECT` |

> **Note:** Reported violations can occasionally be caused by dropped trace events rather than an actual application violation. If a result is unexpected, check the trace for event loss before treating the violation as conclusive.

### `PROJECT_SCHEDULER`

```bash
python3 tessla_verify.py generate integrity delay scheduler \
    --max-tasks 5 \
    --combined \
    --mode MODE \
    --rust \
    --tessla-jar tessla.jar
```

### `PROJECT_DELAY`

```bash
python3 tessla_verify.py generate integrity delay scheduler \
    --max-tasks 3 \
    --combined \
    --mode MODE \
    --rust \
    --tessla-jar tessla.jar
```

### `PROJECT_SEMAPHORE`

```bash
python3 tessla_verify.py generate integrity delay scheduler semaphore \
    --max-tasks 5 \
    --max-semaphores 3 \
    --combined \
    --mode MODE \
    --rust \
    --tessla-jar tessla.jar
```

### `PROJECT_MUTEX`

```bash
python3 tessla_verify.py generate integrity delay scheduler semaphore mutex \
    --max-tasks 6 \
    --max-semaphores 2 \
    --max-mutexes 1 \
    --combined \
    --mode MODE \
    --rust \
    --tessla-jar tessla.jar
```

### `PROJECT_QUEUE`

```bash
python3 tessla_verify.py generate integrity delay scheduler queue \
    --max-tasks 4 \
    --queue 1:1 \
    --combined \
    --mode MODE \
    --rust \
    --tessla-jar tessla.jar
```

### `PROJECT_SENSOR`

```bash
python3 tessla_verify.py generate integrity delay scheduler semaphore queue \
    --max-tasks 3 \
    --max-semaphores 1 \
    --queue 1:8 \
    --queue 2:96 \
    --combined \
    --mode MODE \
    --rust \
    --tessla-jar tessla.jar
```

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

The selected scenario marks itself passed after all required interactions have completed. Tasks then remain blocked or repeat the tested cycle; they never return from their entry functions.

Tracing may remain enabled. SystemView and TeSSLa events provide the detailed scheduling and object-operation sequence while the volatile result gives a simple debugger-visible verdict.

## C checks and trace checks

The C assertions check API return values and data observations directly:

- scheduler startup order, four peer handshakes, and low-task progress only after both peers enter their blocking park;
- binary semaphore empty/full/consume behavior, low-first concurrent blocking, high-priority wake-up, finite acquire timeout, counting to three, overflow, complete token removal, and empty no-wait rejection;
- mutex owner-only unlock, non-recursive rejection, finite lock timeout, low-first concurrent blocking, high-priority ownership handoff, reuse, and `active_owners == 1` after every successful lock;
- queue direct handoff, empty blocking receive, full no-wait rejection, blocking send wake-up, FIFO, full comparison of all message fields, empty no-wait rejection, and finite send/receive timeouts;
- no observer progress during `os_delay_busy()` and observer progress during `os_delay()`.

The following temporal properties require TeSSLa evaluation of the existing trace events and are not claimed by C assertions alone:

- a higher-priority READY task is always selected before a lower-priority one;
- both scheduler peers remain READY across several ticks, every one-tick slice rotates while the equal-priority peer is READY, and multiple rotations occur;
- idle executes only while no normal task is READY;
- task-state transitions are valid and a BLOCKED task never executes;
- `os_delay_busy()` causes no task-state transition, while `os_delay()` keeps the controller non-READY until the requested tick count and makes it READY when that interval expires.

Queue trace events expose only a 32-bit message hash. TeSSLa therefore checks hash integrity; the C test performs the authoritative full-field comparison.
