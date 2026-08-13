# Project Transmission Timing Verification

Verifies the tick interval between observed `transmission_complete` events.

## Checks

- Every completion interval must remain within `target_interval_ticks +/- jitter_ticks`, including both boundary values.
- The first completion is measured from accumulated tick count zero; later completions are measured from the preceding completion.
- With a target of 100 ticks and jitter of 5 ticks, intervals from 95 through 105 ticks are valid; smaller or larger intervals emit `violation_transmission_interval`.

## Implementation Trace

- The application emits `TRANSMISSION_COMPLETE` immediately before transmitting one completed UART data record.
- The RTT receiver converts that record to the `transmission_complete` input stream.
- Positive `TICK` values advance the accumulated OS tick count; non-positive values do not.
- Violation mode exposes `violation_transmission_interval`; check mode exposes `FAIL_transmission_interval` and `PASS_transmission_interval` for each completion.

## Limits

- Timing is expressed only in emitted OS ticks; sub-tick timing and UART transmission duration are not measured.
- The first interval includes all traced ticks before the first completion.
- Missing RTT records make timing results inconclusive and must be detected by the trace-integrity monitor.
- `target_interval_ticks` must be positive; `jitter_ticks` must be non-negative and no greater than the target interval.