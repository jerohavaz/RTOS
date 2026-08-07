# TeSSLa Verification

## RTT → TeSSLa

Record RTT channel 0 to a trace file:

```bash
python3 rtt_to_tessla.py -o trace.input
```

Select another RTT channel if required:

```bash
python3 rtt_to_tessla.py --channel 1 -o trace.input
```

File output prints a received/dropped event summary by default. Disable it with
`--no-summary`.

Stream RTT output directly into TeSSLa:

```bash
python3 rtt_to_tessla.py --stdout | \
java -jar ~/Desktop/tessla.jar interpreter build/combined.tessla
```

The converter emits `trace_incomplete` when sequenced RTT records are missing.
Any integrity violation makes the affected trace inconclusive.

## Verification Modules

| Module | Purpose |
| --- | --- |
| `scheduler` | Task-state transitions, scheduling priority and round-robin behavior. |
| `delay` | Busy-delay duration and non-blocking behavior. |
| `semaphore` | Counting/binary semaphore capacity, blocking, timeout and waiter ordering. |
| `mutex` | Mutex ownership, non-recursive locking, blocking, timeout and waiter handoff. |
| `integrity` | Missing or discontinuous trace-record detection. |

The semaphore and mutex modules assign runtime object addresses dynamically to
bounded monitor slots. Configure these bounds using `--max-semaphores` and
`--max-mutexes` when generating a specification.

## Verification CLI

### List Modules

```bash
python3 tessla_verify.py list
```

### Generate Specifications

Generate all modules separately:

```bash
python3 tessla_verify.py generate
```

Generate a specific module:

```bash
python3 tessla_verify.py generate scheduler
python3 tessla_verify.py generate semaphore --max-semaphores 2
python3 tessla_verify.py generate mutex --max-mutexes 2
```

Generate one combined specification:

```bash
python3 tessla_verify.py generate --combined
```

Combine selected modules:

```bash
python3 tessla_verify.py generate scheduler delay semaphore mutex integrity --combined
```

Options:

```text
--mode {violations,checks}   Output mode (default: violations)
--combined                   Write one build/combined.tessla
--max-tasks N                Override configured task count
--max-semaphores N           Override tracked semaphore-instance count
--max-mutexes N              Override tracked mutex-instance count
--quantum N                  Override configured scheduler quantum
--compile-rust               Compile generated specifications to Rust monitors
--tessla-jar PATH            TeSSLa JAR used for Rust compilation
```

Overrides are applied only to modules that support them.

### Compile a Rust Monitor

Compile a generated module to a native Rust monitor:

```bash
python3 tessla_verify.py generate mutex \
    --max-mutexes 2 \
    --compile-rust \
    --tessla-jar /path/to/tessla.jar
```

This creates:

```text
build/mutex.tessla
build/mutex-monitor
```

Compile a combined monitor:

```bash
python3 tessla_verify.py generate \
    scheduler delay semaphore mutex integrity \
    --combined \
    --compile-rust \
    --tessla-jar /path/to/tessla.jar
```

The resulting executable is `build/combined-monitor`.

### Run Tests

Run all module tests with the interpreter:

```bash
python3 tessla_verify.py test
```

Run selected module tests:

```bash
python3 tessla_verify.py test scheduler semaphore mutex integrity
```

Compile each selected module once and reuse its Rust monitor for all fixtures:

```bash
python3 tessla_verify.py test mutex \
    --rust \
    --tessla-jar /path/to/tessla.jar
```

Options:

```text
--tessla-jar PATH    Path to tessla.jar
--verbose            Print output from passing tests
--rust               Compile once per module and use the Rust monitor
```

Test parameters are taken from each module's `config.py`.

### Clean Generated Specifications and Monitors

```bash
python3 tessla_verify.py clean
```

This removes generated `.tessla` specifications and compiled `*-monitor`
executables from `build/`.

## Verify a Recorded Trace

Using the interpreter:

```bash
python3 tessla_verify.py generate --combined

java -jar ~/Desktop/tessla.jar interpreter \
    build/combined.tessla \
    trace.input
```

Using a compiled Rust monitor:

```bash
python3 tessla_verify.py generate --combined \
    --compile-rust \
    --tessla-jar /path/to/tessla.jar

build/combined-monitor < trace.input
```