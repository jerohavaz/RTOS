# Trace Integrity Verification

Detects record loss reported by the RTT-to-TeSSLa converter.

## Checks

- A positive `trace_incomplete` value emits `violation_trace_incomplete` and gives the number of missing records.

## Implementation Trace

- Verifier-relevant RTT events use the sequenced `TRACE` emitter. A dropped non-blocking write leaves a detectable sequence gap.

## Limits

- The monitor detects only gaps reported through `trace_incomplete`.
- A final dropped record has no later sequence number to expose the gap.
- Results from a trace with reported loss are inconclusive.
