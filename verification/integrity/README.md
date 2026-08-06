# Trace Integrity TeSSLa Verification

This monitor verifies that the recorded trace contains no detected event loss. Verification is based on sequence gaps reported by the RTT-to-TeSSLa converter.

## Verified Properties

### Trace Integrity

* Every emitted trace record must be received in sequence.
* A positive `trace_incomplete` value indicates one or more missing records.
* A trace containing missing records is considered incomplete.

If `violation_trace_incomplete` is emitted, the verification result is inconclusive. Other property results from the same trace must not be treated as valid passes or failures.

## Test Suite

The integrity test suite provides dedicated traces for:

* A complete trace without detected drops
* A single sequence gap
* Multiple sequence gaps

## Integrity Event

```text
trace_incomplete = number of missing trace records
```