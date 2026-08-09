# TeSSLa Verification

## RTT to TeSSLa

Record RTT channel 0 to a trace file:

```bash
python3 rtt_to_tessla.py -o trace.input
```

Select another RTT channel:

```bash
python3 rtt_to_tessla.py --channel 1 -o trace.input
```

File output prints a received/dropped event summary by default. Disable it with
`--no-summary`.

The converter emits `trace_incomplete` when sequenced RTT records are missing.
An integrity violation makes the affected trace inconclusive.

## Verification Modules

| Module | Purpose |
| --- | --- |
| `scheduler` | Task-state transitions, scheduling priority, and round-robin behavior. |
| `delay` | Busy-delay duration and non-blocking behavior. |
| `semaphore` | Counting/binary semaphore capacity, blocking, timeout, and waiter ordering. |
| `mutex` | Mutex ownership, non-recursive locking, blocking, timeout, and waiter handoff. |
| `integrity` | Missing or discontinuous trace-record detection. |

Semaphore and mutex objects are identified by their runtime addresses and
assigned dynamically to bounded monitor slots.

## Verification CLI

### List Modules

```bash
python3 tessla_verify.py list
```

### Generate Specifications

Generation requires every configuration option used by the selected modules.

| Selected module | Required options |
| --- | --- |
| `integrity` | None |
| `delay` | `--max-tasks` |
| `scheduler` | `--max-tasks`, `--quantum` |
| `semaphore` | `--max-tasks`, `--max-semaphores` |
| `mutex` | `--max-tasks`, `--max-mutexes` |

Generate a scheduler specification:

```bash
python3 tessla_verify.py generate scheduler \
    --max-tasks 3 \
    --quantum 1
```

Generate a semaphore specification:

```bash
python3 tessla_verify.py generate semaphore \
    --max-tasks 3 \
    --max-semaphores 2
```

Generate a mutex specification:

```bash
python3 tessla_verify.py generate mutex \
    --max-tasks 3 \
    --max-mutexes 2
```

Generate all modules separately:

```bash
python3 tessla_verify.py generate \
    --max-tasks 3 \
    --quantum 1 \
    --max-semaphores 2 \
    --max-mutexes 2
```

Generate one combined specification containing all modules:

```bash
python3 tessla_verify.py generate --combined \
    --max-tasks 3 \
    --quantum 1 \
    --max-semaphores 2 \
    --max-mutexes 2
```

Generate a combined specification containing selected modules:

```bash
python3 tessla_verify.py generate scheduler mutex integrity \
    --combined \
    --max-tasks 3 \
    --quantum 1 \
    --max-mutexes 2
```

Generation options:

```text
--mode {violations,checks}   Output mode (default: violations)
--combined                   Write one build/combined.tessla
--max-tasks N                Configured task count
--quantum N                  Scheduler quantum in ticks
--max-semaphores N           Tracked semaphore-instance count
--max-mutexes N              Tracked mutex-instance count
--rust                       Compile generated specifications to native monitors
--tessla-jar PATH            Path to tessla.jar used by --rust
```

## Compile Native Rust Monitors

Compile a mutex specification to a native monitor:

```bash
python3 tessla_verify.py generate mutex \
    --max-tasks 3 \
    --max-mutexes 2 \
    --rust \
    --tessla-jar ~/Desktop/tessla.jar
```

This creates:

```text
build/mutex.tessla
build/mutex-monitor
```

Compile one combined monitor containing all modules:

```bash
python3 tessla_verify.py generate --combined \
    --max-tasks 3 \
    --quantum 1 \
    --max-semaphores 2 \
    --max-mutexes 2 \
    --rust \
    --tessla-jar ~/Desktop/tessla.jar
```

This creates:

```text
build/combined.tessla
build/combined-monitor
```

The wrapper suppresses Rust warning diagnostics while retaining compilation
progress and actual compiler errors.

## Run Tests

Tests use the values in each module's `config.py`. Generator configuration
options such as `--max-tasks` and `--max-mutexes` are not required for tests.

Run all tests with the TeSSLa interpreter:

```bash
python3 tessla_verify.py test
```

Run selected tests with the interpreter:

```bash
python3 tessla_verify.py test scheduler semaphore mutex integrity
```

Compile one Rust monitor per selected module and reuse it for all fixtures:

```bash
python3 tessla_verify.py test mutex \
    --rust \
    --tessla-jar ~/Desktop/tessla.jar
```

Test options:

```text
--tessla-jar PATH   Path to tessla.jar
--verbose           Print monitor output from passing tests
--rust              Compile once per module and use the native monitor
```

The selected backend is printed before the tests start:

```text
[BACKEND] TeSSLa interpreter
```

or:

```text
[BACKEND] Rust
```

## Clean Generated Files

```bash
python3 tessla_verify.py clean
```

This removes generated `.tessla` specifications and compiled `*-monitor`
executables from `build/`.

## Verify a Recorded Trace

### Using the TeSSLa Interpreter

First generate the required specification:

```bash
python3 tessla_verify.py generate mutex \
    --max-tasks 3 \
    --max-mutexes 2
```

Run the specification against a recorded trace:

```bash
java -jar ~/Desktop/tessla.jar interpreter \
    build/mutex.tessla \
    trace.input
```

For a combined specification:

```bash
python3 tessla_verify.py generate --combined \
    --max-tasks 3 \
    --quantum 1 \
    --max-semaphores 2 \
    --max-mutexes 2
```

```bash
java -jar ~/Desktop/tessla.jar interpreter \
    build/combined.tessla \
    trace.input
```

### Using a Compiled Rust Monitor

Generate and compile a mutex monitor:

```bash
python3 tessla_verify.py generate mutex \
    --max-tasks 3 \
    --max-mutexes 2 \
    --rust \
    --tessla-jar ~/Desktop/tessla.jar
```

Run it with a trace file redirected to standard input:

```bash
build/mutex-monitor < trace.input
```

The equivalent pipe is:

```bash
cat trace.input | build/mutex-monitor
```

Generate and compile a combined monitor:

```bash
python3 tessla_verify.py generate --combined \
    --max-tasks 3 \
    --quantum 1 \
    --max-semaphores 2 \
    --max-mutexes 2 \
    --rust \
    --tessla-jar ~/Desktop/tessla.jar
```

Run it against a recorded trace:

```bash
build/combined-monitor < trace.input
```

## Process Standard Input Directly

A compiled monitor reads TeSSLa input events from standard input.

Example with manually supplied events:

```bash
printf '%s\n' \
    '0: trace_incomplete = 0' \
    '1: MUTEX_CREATE = (100)' \
    | build/mutex-monitor
```

Example using an existing producer:

```bash
some_trace_producer | build/mutex-monitor
```

## Stream Live RTT Data

### RTT Into the Interpreter

```bash
python3 rtt_to_tessla.py --stdout | \
java -jar ~/Desktop/tessla.jar interpreter build/mutex.tessla
```

For the combined specification:

```bash
python3 rtt_to_tessla.py --stdout | \
java -jar ~/Desktop/tessla.jar interpreter build/combined.tessla
```

### RTT Into a Compiled Monitor

```bash
python3 rtt_to_tessla.py --stdout | build/mutex-monitor
```

For the combined monitor:

```bash
python3 rtt_to_tessla.py --stdout | build/combined-monitor
```

The compiled monitor and interpreter accept the same TeSSLa event format on
standard input.