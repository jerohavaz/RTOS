# TeSSLa Verification

This directory contains tools for converting SEGGER RTT records into TeSSLa
input, generating verification specifications, compiling native monitors, and
running module test suites.

## Verification Modules

Each module documents its verified properties, required trace events, test
coverage, configuration, and limitations in its own README.

| Module | Description |
| --- | --- |
| [Scheduler](scheduler/README.md) | Task states, priorities, scheduling, and round-robin behavior |
| [Message queues](queue/README.md) | Capacity, FIFO order, message integrity, blocking, wake-up, and timeout behavior |
| [Semaphores](semaphore/README.md) | Counts, blocking, direct handoff, wake-up order, and timeouts |
| [Mutexes](mutex/README.md) | Ownership, non-recursive locking, blocking, timeout, and waiter handoff |
| [Delays](delay/README.md) | Busy-delay duration and task-state behavior |
| [Trace integrity](integrity/README.md) | Missing or out-of-order RTT records |

Semaphore and mutex objects are identified by their runtime addresses and
assigned dynamically to bounded monitor slots.

Queue IDs, capacities, and the number of modeled tasks must be known when the
queue specification is generated.

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

The TeSSLa JAR has no built-in default path. Commands that execute TeSSLa must
receive it through either the `TESSLA_JAR` environment variable or
`--tessla-jar PATH`.

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

Record RTT channel 0 to a file:

```bash
python3 rtt_to_tessla.py -o trace.input
```

Select another RTT channel:

```bash
python3 rtt_to_tessla.py --channel 1 -o trace.input
```

File output prints received and dropped event totals by default. Disable the
summary with:

```bash
python3 rtt_to_tessla.py --no-summary -o trace.input
```

The converter emits `trace_incomplete` when sequenced RTT records are missing.
Verification results affected by missing events must be treated as inconclusive.
See [Trace integrity](integrity/README.md) for details.

## Generate Specifications

Generation requires every configuration option used by the selected modules.

| Selected module | Required options |
| --- | --- |
| `integrity` | None |
| `delay` | `--max-tasks` |
| `scheduler` | `--max-tasks`, `--quantum` |
| `semaphore` | `--max-tasks`, `--max-semaphores` |
| `mutex` | `--max-tasks`, `--max-mutexes` |
| `queue` | `--max-tasks`, one or more `--queue QUEUE_ID:CAPACITY` options |

`--max-tasks` is the number of task IDs modeled by the monitor and includes the
idle-task ID. If the application creates nine tasks with IDs `1..9` and the
idle task has ID `0`, use:

```text
--max-tasks 10
```

### Scheduler

```bash
python3 tessla_verify.py generate scheduler \
    --max-tasks 3 \
    --quantum 1
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

Generate a monitor for queue `1` with capacity `2` and queue `4` with capacity
`8`:

```bash
python3 tessla_verify.py generate queue \
    --max-tasks 3 \
    --queue 1:2 \
    --queue 4:8
```

Queue IDs must be non-negative and capacities must be greater than zero.
Duplicate queue IDs are rejected.

See [Message queue verification](queue/README.md) for the queue trace contract
and current limitations.

### Integrity

```bash
python3 tessla_verify.py generate integrity
```

### Generate all modules separately

```bash
python3 tessla_verify.py generate \
    --max-tasks 3 \
    --quantum 1 \
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
    --quantum 1 \
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
    --quantum 1 \
    --max-mutexes 2 \
    --queue 1:2 \
    --queue 4:8
```

## Generation Options

```text
--mode {violations,checks}   Output mode (default: violations)
--combined                   Write one build/combined.tessla
--max-tasks N                Number of modeled task IDs, including idle
--quantum N                  Scheduler quantum in ticks
--max-semaphores N           Number of tracked semaphore instances
--max-mutexes N              Number of tracked mutex instances
--queue ID:CAPACITY          Queue ID and capacity; repeat for multiple queues
--rust                       Compile generated specifications to native monitors
--tessla-jar PATH            TeSSLa JAR used by --rust
```

Options are applied only to modules that support them.

## Compile Native Rust Monitors

Rust compilation requires either `TESSLA_JAR` or `--tessla-jar PATH`.

Compiling a Rust monitor can be slow, especially on the first build while Rust
dependencies are prepared. Once compiled, the native monitor generally
processes traces faster than the TeSSLa interpreter. The `test --rust` command
compiles each selected module once and reuses that executable for all of the
module's test traces.

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
    --quantum 1 \
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

The wrapper suppresses Rust warning diagnostics while retaining compilation
progress and actual compiler errors.

## Run Tests

Tests use the configuration and expected violations from each module's
`config.py`. Generator options such as `--max-tasks`, `--max-mutexes`, and
`--queue` are not required when running tests.

Tests always require a TeSSLa JAR, supplied through `TESSLA_JAR` or
`--tessla-jar PATH`.

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
    --quantum 1 \
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
    --quantum 1 \
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

The compiled monitor reads TeSSLa events from standard input. This is
equivalent:

```bash
cat trace.input | build/combined-monitor
```

## Stream Live RTT Data

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

The compiled monitor and interpreter accept the same TeSSLa event format on
standard input.

## Clean Generated Files

```bash
python3 tessla_verify.py clean
```

This removes generated `.tessla` specifications and compiled `*-monitor`
executables from `build/`.