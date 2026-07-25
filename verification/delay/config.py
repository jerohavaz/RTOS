GENERATOR_OPTIONS = {
    "max_tasks": 3,
}

EXPECTED = {
    "valid_busy_delay.input": set(),
    "bad_busy_delay_blocked.input": {
        "violation_busy_delay_blocked",
    },
}