GENERATOR_OPTIONS = {
    "max_tasks": 3,
    "quantum_ticks": 1,
}


EXPECTED = {
    "bad_blocked_event_inconsistent.input": {
        "violation_blocked_event_inconsistent",
    },
    "bad_blocked_running.input": {
        "violation_blocked_running",
        "violation_running_event_inconsistent",
    },
    "bad_idle_while_ready.input": {
        "violation_idle_while_ready",
    },
    "bad_invalid_transition.input": {
        "violation_invalid_state_transition",
    },
    "bad_priority.input": {
        "violation_priority",
    },
    "bad_ready_event_inconsistent.input": {
        "violation_ready_event_inconsistent",
    },
    "bad_rr_quantum.input": {
        "violation_quantum",
        "violation_round_robin",
    },
    "bad_running_event_inconsistent.input": {
        "violation_running_event_inconsistent",
    },
    "bad_running_id_out_of_range.input": {
        "violation_running_id_out_of_range",
    },
    "valid_baseline.input": set(),
    "valid_round_robin.input": set(),
}