# TeSSLa Verification

## RTT → TeSSLa

Record RTT output to a TeSSLa trace file:

```bash
python3 rtt_to_tessla.py -o trace.input
```

Or stream RTT output directly into the TeSSLa interpreter:

```bash
python3 rtt_to_tessla.py --stdout | \
java -jar ~/Desktop/tessla.jar interpreter build/scheduler.tessla
```

---

## Verification CLI

### List Available Modules

```bash
python3 verify.py list
```

### Generate Specifications

Generate all verification modules:

```bash
python3 verify.py generate
```

Generate a specific module:

```bash
python3 verify.py generate scheduler
```

Available options:

```text
--mode {violations,checks}   Output mode (default: violations)
--max-tasks N                Override configured task count
--quantum N                  Override configured scheduler quantum
```

Example:

```bash
python3 verify.py generate \
    scheduler \
    --mode checks \
    --max-tasks 13 \
    --quantum 1
```

Generated specifications are written to:

```text
build/
```

### Run Tests

Run all verification tests:

```bash
python3 verify.py test
```

Run a specific module:

```bash
python3 verify.py test scheduler
```

Available options:

```text
--tessla-jar PATH    Path to tessla.jar
--verbose            Print interpreter output for passing tests
```

Test generation parameters (e.g. `max_tasks`, `quantum_ticks`) are taken from each module's `config.py`.

### Clean Generated Specifications

```bash
python3 verify.py clean
```

---

## Manual Verification

Generate a scheduler specification for the target RTOS configuration:

```bash
python3 verify.py generate \
    scheduler \
    --mode checks \
    --max-tasks 13 \
    --quantum 1
```

Verify a recorded trace:

```bash
java -jar ~/Desktop/tessla.jar interpreter \
    build/scheduler.tessla \
    trace.input
```

TODO: POTENTIALLY ADD DROP DETECTION
TODO: CHECK ARG ERRORS WHEN GENERATING MULTIPLE MODULES