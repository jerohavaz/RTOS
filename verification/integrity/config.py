GENERATOR_OPTIONS = {}


EXPECTED = {
    "valid_complete_trace.input": set(),
    "bad_dropped_records.input": {
        "violation_trace_incomplete",
    },
    "bad_multiple_drop_gaps.input": {
        "violation_trace_incomplete",
    },
}
