GENERATOR_OPTIONS = {
    "target_interval_ticks": 100,  # Gewünschtes Zeitintervall (z.B. 100 ms oder 50 ms)
    "tolerance_ticks": 1,          # Zulässige Abweichung in ms (z.B. ±1 ms)
}

EXPECTED = {
    "valid_project_transmission.input": set(),
    "bad_project_timeout.input": {
        "violation_sensor_interval_deviation",
    },
}