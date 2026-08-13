GENERATOR_OPTIONS = {
    "target_interval_ticks": 100,
    "tolerance_ticks": 5,
}

EXPECTED = {
    "valid_project_transmission.input": set(),
    "bad_project_timeout.input": {
        "violation_transmission_interval_deviation",
    },
}