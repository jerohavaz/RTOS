GENERATOR_OPTIONS = {
    "max_tasks": 3,
    "busy_tasks": 3, 
}

EXPECTED = {
    "valid_busy_delay.input": set(),
    "bad_busy_delay_blocked.input": {
        "violation_busy_delay_blocked",
    },
    "bad_busy_delay_too_short.input": {
        "violation_busy_delay_too_short",
    },
}