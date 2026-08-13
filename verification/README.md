# TeSSLa Verification

This directory contains tools for converting SEGGER RTT records into TeSSLa input, generating verification specifications, compiling native monitors, and running module test suites.

## Verification Modules

Each module documents its verified properties, required trace events, test coverage, configuration, and limitations in its own README.

| Module | Description |
| --- | --- |
| [Scheduler](scheduler_spec/README.md) | Task states, priorities, scheduling, and round-robin behavior |
| [Message queues](queue_spec/README.md) | Capacity, FIFO order, message integrity, blocking, wake-up, and timeout behavior |
| [Semaphores](semaphore_spec/README.md) | Counts, blocking, direct handoff, wake-up order, and timeouts |
| [Mutexes](mutex_spec/README.md) | Ownership, non-recursive locking, blocking, timeout, and waiter handoff |
| [Delays](delay_spec/README.md) | Busy-delay duration and task-state behavior |
| [Trace integrity](integrity_spec/README.md) | Missing or out-of-order RTT records |

Semaphore and mutex objects are identified by their runtime addresses and assigned dynamically to bounded monitor slots.

Queue IDs, capacities, and the number of modeled tasks must be known when the queue specification is generated.

List the modules recognized by the CLI:

```bash
python3 tessla_verify.py list
```

## Requirements

Base requirements:

- Python 3.12 or newer
- Java 21 or newer
- A TeSSLa JAR
- A SEGGER RTT connection when recording live target data

Native Rust monitor compilation additionally requires:

- `rustc`
- `cargo`
- A native linker and build toolchain

The TeSSLa JAR has no built-in default path. Commands that execute TeSSLa must receive it through either the `TESSLA_JAR` environment variable or `--tessla-jar PATH`.

Set the environment variable for the current shell:

```bash
export TESSLA_JAR=/path/to/tessla.jar
```

Alternatively, pass the path to an individual command:

```bash
python3 tessla_verify.py test mutex \
    --tessla-jar /path/to/tessla.jar
```

Plain specification generation without `--rust` does not require a TeSSLa JAR.

## Capture a Trace

### Attach RTT before resuming the debugger

For a complete live trace, attach the RTT collector before the firmware starts running.

Recommended debugger workflow:

1. Start the STM32 using the debugger.
2. Wait until execution stops at the `HAL_Init()` breakpoint.
3. Start `rtt_to_tessla.py` and wait until it has connected to the RTT stream.
4. Resume execution in the debugger.
5. Confirm that `Trace session 1 started.` appears when the firmware emits the binary session-start record.

Attaching while the target is still halted ensures that the collector is already listening when tracing starts, so the first events of the firmware run are not missed.

For file output with `-o`, the same attach-before-resume workflow is recommended when a complete trace is required. A new session-start record also resets the converter's sequence state and truncates the output file to the current target session.

The firmware initializes RTT during startup with `SEGGER_RTT_Init()` before the RTOS starts and must not reinitialize RTT during normal execution.

Record RTT channel 0 to a file:

```bash
python3 rtt_to_tessla.py -o trace.input
```

Select another RTT channel:

```bash
python3 rtt_to_tessla.py --channel 1 -o trace.input
```

File output prints received and dropped event totals by default. Disable the summary with:

```bash
python3 rtt_to_tessla.py --no-summary -o trace.input
```

The converter emits `trace_incomplete` when sequenced RTT records are missing. Verification results affected by missing events must be treated as inconclusive. See [Trace integrity](integrity_spec/README.md) for details.

The RTT up-buffer is 64 KiB and remains non-blocking. The static task array is placed in the 32 KiB SRAM2 bank so the larger RTT buffer does not consume the task-stack budget in the 96 KiB primary SRAM bank. This absorbs short RTT discovery and host-polling stalls but cannot guarantee lossless capture when the enabled event categories continuously produce data faster than the debug probe drains it. The receiver continuously drains raw socket bytes on a dedicated thread into an unbounded host-memory queue so TeSSLa conversion and file output cannot back-pressure the J-Link RTT connection. If gaps remain at an 8 MHz or faster SWD clock, inspect the probe's RTT polling behavior before changing trace categories; reducing the event set changes which properties can be verified. A sequence gap confirms target-side RTT buffer exhaustion because `SEGGER_RTT_WriteSkipNoLock()` discards the complete record when it cannot fit.

### Binary RTT protocol

The firmware writes each event with one non-blocking `SEGGER_RTT_WriteSkipNoLock()` call. Every multi-byte value is little-endian, and records are concatenated without line delimiters:

| Field | Type | Description |
| --- | --- | --- |
| `sequence` | `u16` | Increments for every attempted record and wraps modulo 65536 |
| `event_id` | `u8` | Numeric event identifier listed below |
| `payload_length` | `u8` | Number of payload bytes following the header |
| `payload` | bytes | Event fields in table order |

`rtt_to_tessla.py` reads raw socket chunks into a byte buffer, waits for a `SESSION_START` header when capturing live data, and removes a record only after all `4 + payload_length` bytes are available. It compares consecutive `u16` sequence values modulo 65536 and emits `trace_incomplete` for each gap. `--stdin` accepts the same binary record stream and assumes the first byte is record-aligned.

Payload notation uses `B` for `u8`, `I` for `u32`, and `-` for an empty payload:

| IDs | Events and payloads |
| --- | --- |
| 0-8 | `SESSION_START -`, `TASK_CREATE BB`, `STATE BBB`, `READY BB`, `RUNNING BB`, `STOP_RUNNING -`, `BLOCKED B`, `IDLE -`, `TICK I` |
| 9-12 | `DELAY_BUSY_START BI`, `DELAY_BUSY_END B`, `DELAY_START BI`, `DELAY_END B` |
| 13-19 | `SEM_CREATE III`, `SEM_ACQUIRE_ENTER IBIIB`, `SEM_ACQUIRE_EXIT IBIB`, `SEM_BLOCK IBBIB`, `SEM_TIMEOUT IBI`, `SEM_RELEASE IIIIB`, `SEM_WAKE IBB` |
| 20-26 | `MUTEX_CREATE I`, `MUTEX_LOCK_ENTER IBBIB`, `MUTEX_LOCK_EXIT IBBB`, `MUTEX_BLOCK IBBBIB`, `MUTEX_TIMEOUT IBB`, `MUTEX_UNLOCK IBBBB`, `MUTEX_WAKE IBB` |
| 27-35 | `QUEUE_CREATE II`, `QUEUE_SEND_ATTEMPT IBBII`, `QUEUE_SEND_SUCCESS IBI`, `QUEUE_SEND_BLOCK IBB`, `QUEUE_SEND_TIMEOUT IB`, `QUEUE_RECV_ATTEMPT IBBI`, `QUEUE_RECV_SUCCESS IBI`, `QUEUE_RECV_BLOCK IBB`, `QUEUE_RECV_TIMEOUT IB` |
| 36-41 | `QUEUE_WAKE_SEND IB`, `QUEUE_WAKE_RECV IB`, `QUEUE_HANDOFF IBBI`, `QUEUE_FILL II`, `TRANSMISSION_COMPLETE -`, `LOG bytes` |

The session-start record is exactly `00 00 00 00`: sequence 0, event ID 0, and an empty payload. The next record has sequence 1. Task IDs, priorities, task states, and booleans use `u8`; addresses, queue IDs, counts, tick values, timeouts, and hashes retain `u32`.

## Generate Specifications

Generation requires every configuration option used by the selected modules.

| Selected module | Required options |
| --- | --- |
| `integrity` | None |
| `delay` | `--max-tasks` |
| `scheduler` | `--max-tasks` |
| `semaphore` | `--max-tasks`, `--max-semaphores` |
| `mutex` | `--max-tasks`, `--max-mutexes` |
| `queue` | `--max-tasks`, one or more `--queue QUEUE_ID:CAPACITY` options |

`--max-tasks` is the number of task IDs modeled by the monitor and includes the idle-task ID. If the application creates nine tasks with IDs `1..9` and the idle task has ID `0`, use:

```text
--max-tasks 10
```

### Scheduler

```bash
python3 tessla_verify.py generate scheduler --max-tasks 3
```

### Delay

```bash
python3 tessla_verify.py generate delay \
    --max-tasks 3
```

### Semaphore

```bash
python3 tessla_verify.py generate semaphore \
    --max-tasks 3 \
    --max-semaphores 2
```

### Mutex

```bash
python3 tessla_verify.py generate mutex \
    --max-tasks 3 \
    --max-mutexes 2
```

### Message queue

Generate a queue monitor for queue `1` with capacity `2`:

```bash
python3 tessla_verify.py generate queue \
    --max-tasks 3 \
    --queue 1:2
```

Generate a monitor for queue `1` with capacity `2` and queue `4` with capacity `8`:

```bash
python3 tessla_verify.py generate queue \
    --max-tasks 3 \
    --queue 1:2 \
    --queue 4:8
```

Queue IDs must be non-negative and capacities must be greater than zero. Duplicate queue IDs are rejected.

See [Message queue verification](queue_spec/README.md) for the queue trace contract and current limitations.

### Integrity

```bash
python3 tessla_verify.py generate integrity
```

### Generate all modules separately

```bash
python3 tessla_verify.py generate \
    --max-tasks 3 \
    --max-semaphores 2 \
    --max-mutexes 2 \
    --queue 1:2 \
    --queue 4:8
```

### Generate one combined specification

Generate one combined specification containing all modules:

```bash
python3 tessla_verify.py generate --combined \
    --max-tasks 3 \
    --max-semaphores 2 \
    --max-mutexes 2 \
    --queue 1:2 \
    --queue 4:8
```

This creates:

```text
build/combined.tessla
```

Generate a combined specification containing selected modules:

```bash
python3 tessla_verify.py generate scheduler queue mutex integrity \
    --combined \
    --max-tasks 3 \
    --max-mutexes 2 \
    --queue 1:2 \
    --queue 4:8
```

## Generation Options

```text
--mode {violations,checks}   Output mode (default: violations)
--combined                   Write one build/combined.tessla
--max-tasks N                Number of modeled task IDs, including idle
--max-semaphores N           Number of tracked semaphore instances
--max-mutexes N              Number of tracked mutex instances
--queue ID:CAPACITY          Queue ID and capacity; repeat for multiple queues
--rust                       Compile generated specifications to native monitors
--tessla-jar PATH            TeSSLa JAR used by --rust
```

Options are applied only to modules that support them.

## Compile Native Rust Monitors

Rust compilation requires either `TESSLA_JAR` or `--tessla-jar PATH`.

Compiling a Rust monitor can be slow, especially on the first build while Rust dependencies are prepared. Once compiled, the native monitor generally processes traces faster than the TeSSLa interpreter. The `test --rust` command compiles each selected module once and reuses that executable for all of the module's test traces.

### Compile one module

Using `TESSLA_JAR`:

```bash
export TESSLA_JAR=/path/to/tessla.jar

python3 tessla_verify.py generate mutex \
    --max-tasks 3 \
    --max-mutexes 2 \
    --rust
```

Using an explicit path:

```bash
python3 tessla_verify.py generate mutex \
    --max-tasks 3 \
    --max-mutexes 2 \
    --rust \
    --tessla-jar /path/to/tessla.jar
```

This creates:

```text
build/mutex.tessla
build/mutex-monitor
```

Compile a queue specification:

```bash
python3 tessla_verify.py generate queue \
    --max-tasks 3 \
    --queue 1:2 \
    --queue 4:8 \
    --rust \
    --tessla-jar /path/to/tessla.jar
```

This creates:

```text
build/queue.tessla
build/queue-monitor
```

### Compile a combined monitor

```bash
python3 tessla_verify.py generate --combined \
    --max-tasks 3 \
    --max-semaphores 2 \
    --max-mutexes 2 \
    --queue 1:2 \
    --queue 4:8 \
    --rust \
    --tessla-jar /path/to/tessla.jar
```

This creates:

```text
build/combined.tessla
build/combined-monitor
```

The wrapper suppresses Rust warning diagnostics while retaining compilation progress and actual compiler errors.

## Run Tests

Tests use the configuration and expected violations from each module's `config.py`. Generator options such as `--max-tasks`, `--max-mutexes`, and `--queue` are not required when running tests.

Tests always require a TeSSLa JAR, supplied through `TESSLA_JAR` or `--tessla-jar PATH`.

### Interpreter backend

Using `TESSLA_JAR`:

```bash
export TESSLA_JAR=/path/to/tessla.jar
python3 tessla_verify.py test
```

Run selected module tests with an explicit path:

```bash
python3 tessla_verify.py test scheduler queue mutex integrity \
    --tessla-jar /path/to/tessla.jar
```

Use `--verbose` to print interpreter output from passing tests:

```bash
python3 tessla_verify.py test queue \
    --verbose \
    --tessla-jar /path/to/tessla.jar
```

### Rust backend

Compile one Rust monitor per selected module and reuse it for every fixture:

```bash
python3 tessla_verify.py test queue mutex \
    --rust \
    --tessla-jar /path/to/tessla.jar
```

The selected backend is printed before testing:

```text
[BACKEND] TeSSLa interpreter
```

or:

```text
[BACKEND] Rust
```

Test options:

```text
--tessla-jar PATH   Path to tessla.jar; otherwise TESSLA_JAR is required
--verbose           Print monitor output from passing tests
--rust              Compile once per module and use the native monitor
```

## Verify a Recorded Trace

### TeSSLa interpreter

Generate the required specification:

```bash
python3 tessla_verify.py generate mutex \
    --max-tasks 10 \
    --max-mutexes 2
```

Run it using `TESSLA_JAR`:

```bash
export TESSLA_JAR=/path/to/tessla.jar

java -jar "$TESSLA_JAR" interpreter \
    build/mutex.tessla \
    trace.input
```

Generate a combined specification:

```bash
python3 tessla_verify.py generate --combined \
    --max-tasks 10 \
    --max-semaphores 10 \
    --max-mutexes 2 \
    --queue 536871000:4
```

Run it with an explicit JAR path:

```bash
java -jar /path/to/tessla.jar interpreter \
    build/combined.tessla \
    trace.input
```

### Compiled Rust monitor

Generate and compile a mutex monitor:

```bash
python3 tessla_verify.py generate mutex \
    --max-tasks 10 \
    --max-mutexes 2 \
    --rust \
    --tessla-jar /path/to/tessla.jar
```

Run it with a trace file redirected to standard input:

```bash
build/mutex-monitor < trace.input
```

Generate and compile a queue monitor:

```bash
python3 tessla_verify.py generate queue \
    --max-tasks 3 \
    --queue 536871000:4 \
    --rust \
    --tessla-jar /path/to/tessla.jar
```

Run it against a recorded trace:

```bash
build/queue-monitor < trace.input
```

Generate and compile a combined monitor:

```bash
python3 tessla_verify.py generate --combined \
    --max-tasks 10 \
    --max-semaphores 10 \
    --max-mutexes 2 \
    --queue 536871000:4 \
    --rust \
    --tessla-jar /path/to/tessla.jar
```

Run it against a recorded trace:

```bash
build/combined-monitor < trace.input
```

The compiled monitor reads TeSSLa events from standard input. This is equivalent:

```bash
cat trace.input | build/combined-monitor
```

## Stream Live RTT Data

For live verification, start the target under the debugger and wait at the `HAL_Init()` breakpoint. Attach `rtt_to_tessla.py` while the target is halted, then resume execution. This ensures that the collector is already listening when the session-start record and the first trace events are emitted.

### RTT into the TeSSLa interpreter

Using `TESSLA_JAR`:

```bash
python3 rtt_to_tessla.py --stdout | \
java -jar "$TESSLA_JAR" interpreter build/combined.tessla
```

Using an explicit JAR path:

```bash
python3 rtt_to_tessla.py --stdout | \
java -jar /path/to/tessla.jar interpreter build/combined.tessla
```

### RTT into a compiled module monitor

```bash
python3 rtt_to_tessla.py --stdout | \
build/queue-monitor
```

### RTT into a compiled combined monitor

```bash
python3 rtt_to_tessla.py --stdout | \
build/combined-monitor
```

The compiled monitor and interpreter accept the same TeSSLa event format on standard input.

## Clean Generated Files

```bash
python3 tessla_verify.py clean
```

This removes generated `.tessla` specifications and compiled `\*-monitor` executables from `build/`.
