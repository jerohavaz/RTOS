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

## Verification CLI

### List Modules

```bash
python3 verify.py list
```

### Generate Specifications

Generate all modules separately:

```bash
python3 verify.py generate
```

Generate a specific module:

```bash
python3 verify.py generate scheduler
```

Generate one combined specification:

```bash
python3 verify.py generate --combined
```

Combine selected modules:

```bash
python3 verify.py generate scheduler delay integrity --combined
```

Options:

```text
--mode {violations,checks}   Output mode (default: violations)
--combined                   Write one build/combined.tessla
--max-tasks N                Override configured task count
--quantum N                  Override configured scheduler quantum
```

Overrides are applied only to modules that support them.

### Run Tests

Run all module tests:

```bash
python3 verify.py test
```

Run selected module tests:

```bash
python3 verify.py test scheduler integrity
```

Options:

```text
--tessla-jar PATH    Path to tessla.jar
--verbose            Print output from passing tests
```

Test parameters are taken from each module's `config.py`.

### Clean Generated Specifications

```bash
python3 verify.py clean
```

## Verify a Recorded Trace

```bash
python3 verify.py generate --combined

java -jar ~/Desktop/tessla.jar interpreter \
    build/combined.tessla \
    trace.input
```