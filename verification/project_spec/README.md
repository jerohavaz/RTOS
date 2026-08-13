# Project transmission timing verification

This module verifies the tick interval of every `transmission_complete` event.

The user supplies:

- `target_interval_ticks`: expected number of OS ticks between transmission completions.
- `jitter_ticks`: allowed jitter on either side of the target.

The accepted interval is inclusive:

```text
[target_interval_ticks - jitter_ticks,
 target_interval_ticks + jitter_ticks]
```

For example, a target of `100` ticks with `5` ticks of jitter accepts a
`transmission_complete` interval from `95` through `105` ticks. An interval of
`94` or `106` ticks emits `violation_transmission_interval`.

The first `transmission_complete` interval is measured from accumulated tick
count zero. Later intervals are measured from the preceding
`transmission_complete` event.

Only positive `tick` values advance the accumulated tick count, matching the
tick-duration model used by the other verification modules.

## Generate

Using the jitter alias:

```bash
python3 tessla_verify.py generate project \
    --target-interval-ticks 100 \
    --jitter-ticks 5
```

The existing `--tolerance-ticks 5` spelling is also accepted.

Generate check-mode outputs instead of violation-only output:

```bash
python3 tessla_verify.py generate project \
    --target-interval-ticks 100 \
    --jitter-ticks 5 \
    --mode checks
```

Violation mode exposes:

```text
violation_transmission_interval
```

Check mode exposes:

```text
FAIL_transmission_interval
PASS_transmission_interval
```
