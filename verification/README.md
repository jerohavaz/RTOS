# TeSSLa Verification

This directory contains the tooling used to convert SEGGER RTT records into TeSSLa input, generate verification specifications, and run the module test suites.

## Modules

Each module documents its verified properties, required trace events, test coverage, configuration, and limitations in its own README.

| Module | Description |
| --- | --- |
| [Scheduler](scheduler/README.md) | Task states, priorities, scheduling, and round-robin behavior |
| [Message queues](queue/README.md) | Capacity, FIFO order, message integrity, blocking, wake-up, and timeout behavior |
| [Semaphores](semaphore/README.md) | Counts, blocking, direct handoff, wake-up order, and timeouts |
| [Delays](delay/README.md) | Busy-delay duration and task-state behavior |
| [Trace integrity](integrity/README.md) | Missing or out-of-order RTT records |

List the modules recognized by the CLI:

```bash
python3 verify.py list
```

## Requirements

- Python 3.12 or newer
- Java 21 or newer
- A TeSSLa JAR
- A SEGGER RTT connection when recording live target data

Set the TeSSLa JAR path once for the current shell:

```bash
export TESSLA_JAR=/path/to/tessla.jar
```

The `--tessla-jar PATH` option can override this environment variable when running tests.

## Capture a Trace

Record RTT channel 0 to a file:

```bash
python3 rtt_to_tessla.py -o trace.input
```

Select another RTT channel if required:

```bash
python3 rtt_to_tessla.py --channel 1 -o trace.input
```

File output prints received and dropped event totals by default. Use `--no-summary` to disable the summary.

The converter emits `trace_incomplete` when sequenced RTT records are missing. Verification results affected by missing events must be treated as inconclusive. See [Trace integrity](integrity/README.md) for details.

## Generate Specifications

Generate every module separately:

```bash
python3 verify.py generate
```

Generate one module:

```bash
python3 verify.py generate queue
```

Generate one combined specification:

```bash
python3 verify.py generate --combined
```

Selected modules can also be combined:

```bash
python3 verify.py generate scheduler queue integrity --combined
```

Common generation options:

```text
--mode {violations,checks}       Output mode (default: violations)
--combined                       Write build/combined.tessla
--max-tasks N                    Override the configured task count
--max-semaphores N               Override tracked semaphore instances
--quantum N                      Override the scheduler quantum
--queue QUEUE_ID:CAPACITY        Override configured queues; repeat as needed
```

Overrides apply only to modules that support them. Default parameters are stored in each module's `config.py`.

For example, generate a queue monitor for queue `1` with capacity `2` and queue `4` with capacity `8`:

```bash
python3 verify.py generate queue \
    --queue 1:2 \
    --queue 4:8
```

Queue IDs and capacities must be known when the specification is generated. See [Message queue verification](queue/README.md) for the trace contract and current limitations.

## Run Tests

Run every module test using `TESSLA_JAR`:

```bash
python3 verify.py test
```

Run selected module tests:

```bash
python3 verify.py test scheduler queue integrity
```

Pass the JAR path explicitly instead:

```bash
python3 verify.py test queue --tessla-jar /path/to/tessla.jar
```

Use `--verbose` to print interpreter output from passing tests. Test parameters and expected violations come from each module's `config.py`.

## Verify a Recorded Trace

```bash
python3 verify.py generate --combined

java -jar "$TESSLA_JAR" interpreter \
    build/combined.tessla \
    trace.input
```

On Unix systems, live RTT data can be streamed directly into TeSSLa:

```bash
python3 verify.py generate --combined

python3 rtt_to_tessla.py --stdout | \
    java -jar "$TESSLA_JAR" interpreter \
    build/combined.tessla /dev/stdin
```

## Clean Generated Files

```bash
python3 verify.py clean
```
