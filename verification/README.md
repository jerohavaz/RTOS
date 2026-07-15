# TeSSLa Verification

## Trace Input

Record RTT output:

```bash
python3 rtt_to_tessla.py -o trace.input
```

Or stream it directly into a generated monitor:

```bash
python3 rtt_to_tessla.py --stdout | \
java -jar ~/Desktop/tessla.jar interpreter build/combined.tessla
```

## Verification CLI

List modules:

```bash
python3 verify.py list
```

Generate every module separately:

```bash
python3 verify.py generate
```

Generate one module:

```bash
python3 verify.py generate scheduler
python3 verify.py generate queue
```

Generate one specification containing all modules:

```bash
python3 verify.py generate --combined
```

Generation options:

```text
--mode {violations,checks}   Output violations or FAIL/PASS streams
--max-tasks N                Override task count
--quantum N                  Override scheduler quantum
--queue ID:CAPACITY          Override queues; repeat for multiple queues
--combined                   Combine selected modules into one specification
```

Example:

```bash
python3 verify.py generate --combined \
    --mode checks \
    --max-tasks 13 \
    --quantum 1 \
    --queue 1:2 \
    --queue 5:8
```

Generated files are written to `build/`.

## Tests

Run all tests or one module:

```bash
python3 verify.py test
python3 verify.py test scheduler
python3 verify.py test queue
```

Options:

```text
--tessla-jar PATH   Path to tessla.jar
--verbose           Print output from passing tests
```

Tests use the generator options and expected outputs from each module's `config.py`.

## Manual Verification

```bash
python3 verify.py generate --combined

java -jar ~/Desktop/tessla.jar interpreter \
    build/combined.tessla \
    trace.input
```

Clean generated specifications:

```bash
python3 verify.py clean
```