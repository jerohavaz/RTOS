GENERATOR_OPTIONS = {
    "max_latency_ticks": 100,
}

EXPECTED = {
    "valid_project_transmission.input": set(),
    "bad_project_timeout.input": {
        "violation_sensor_timeout",
    },
}