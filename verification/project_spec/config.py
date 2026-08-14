"""Configure project transmission timing monitor generation and expected tests.

Author: Martin
Author: Jerome
"""

GENERATOR_OPTIONS = {
    "target_interval_ticks": 100,
    "jitter_ticks": 5,
}

EXPECTED = {
    "valid_nominal.input": set(),
    "valid_jitter_bounds.input": set(),
    "bad_too_early.input": {
        "violation_transmission_interval",
    },
    "bad_too_late.input": {
        "violation_transmission_interval",
    },
}
