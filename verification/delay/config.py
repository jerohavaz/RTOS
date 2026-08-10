GENERATOR_OPTIONS = {
    "max_tasks": 3,
}

EXPECTED = {
    # --- Busy Delay Test Cases ---
    "valid_busy_delay.input": set(),
    "bad_busy_delay_blocked.input": {
        "violation_busy_delay_blocked",
    },
    "bad_busy_delay_too_short.input": {
        "violation_busy_delay_too_short",
    },
    "bad_busy_delay_not_running.input": {
        "violation_busy_delay_invalid_start_state",
    },
    # --- Non-Blocking os_delay Test Cases ---
    "valid_delay.input": set(),
    "bad_delay_not_blocked.input": {
        "violation_delay_not_blocked",
    },
    "bad_delay_too_short.input": {
        "violation_delay_too_short",
    },
    "bad_delay_invalid_unblock.input": {
        "violation_delay_invalid_unblock_state",
    },
    "bad_delay_missing_blocked_event.input": {
        "violation_delay_not_blocked",
    },
    "bad_delay_missing_ready_event.input": {
        "violation_delay_invalid_unblock_state",
    },
    "valid_busy_delay_preempted.input": set(),
}
