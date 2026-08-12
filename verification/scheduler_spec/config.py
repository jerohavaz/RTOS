"""Configure scheduler monitor generation and expected test results.

Author: Jerome
"""

GENERATOR_OPTIONS = {
    "max_tasks": 3,
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
    "bad_fifo_order.input": {
        "violation_round_robin",
    },
    "bad_priority.input": {
        "violation_priority",
    },
    "bad_priority_field.input": {
        "violation_task_priority_inconsistent",
    },
    "bad_ready_to_blocked.input": {
        "violation_invalid_state_transition",
    },
    "bad_ready_event_inconsistent.input": {
        "violation_ready_event_inconsistent",
    },
    "bad_tick_rotation.input": {
        "violation_round_robin",
        "violation_tick_rotation",
    },
    "bad_state_discontinuity.input": {
        "violation_state_discontinuity",
    },
    "bad_running_event_inconsistent.input": {
        "violation_running_event_inconsistent",
    },
    "bad_running_id_out_of_range.input": {
        "violation_task_id_out_of_range",
    },
    "valid_baseline.input": set(),
    "valid_round_robin.input": set(),
    "valid_three_task_round_robin.input": set(),
}
