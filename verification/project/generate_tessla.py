#!/usr/bin/env python3

CHECKS = [
    (
        "sensor_read_to_transmission_within_interval",
        "sensor_interval_deviation",
    ),
]


def emit_header() -> str:
    return """in sensor_read: Events[Unit]
in transmission_complete: Events[Unit]
in tick: Events[Int]

# --- Globaler Tick-Zähler ---
def tick_sum: Events[Int] =
  merge(
    if tick > 0 then default(last(tick_sum, tick), 0) + tick else 0,
    0
  )

"""


def emit_pass_fail_pair(
    public_name: str,
    internal_name: str,
    trigger_expr: str,
    fail_condition: str,
) -> str:
    return f"""def FAIL_{public_name} :=
  filter({trigger_expr}, {fail_condition})

def PASS_{public_name} :=
  filter({trigger_expr}, {fail_condition} == false)

"""


def emit_latency_check(target_interval_ticks: int, tolerance_ticks: int) -> str:
    min_ticks = target_interval_ticks - tolerance_ticks
    max_ticks = target_interval_ticks + tolerance_ticks

    return f"""# ==================== Sensor Latency Check ====================
# Speichert den Tick-Stand beim Auslösen von sensor_read
def last_read_tick: Events[Int] =
  merge(
    last(tick_sum, sensor_read),
    default(last(last_read_tick, tick), -1)
  )

# Latenzberechnung beim Eintreffen von transmission_complete
def elapsed_ticks: Events[Int] =
  last(tick_sum, transmission_complete) - last(last_read_tick, transmission_complete)

# Bedingung für Intervallabweichung (Erlaubter Bereich: [{min_ticks}, {max_ticks}] Ticks)
def is_interval_violation: Events[Bool] = elapsed_ticks < {min_ticks} || elapsed_ticks > {max_ticks}

# Violation: Nimmt das Event 'transmission_complete' und filtert es basierend auf der Boolean-Bedingung
def violation_sensor_interval_deviation :=
  filter(transmission_complete, is_interval_violation)

{emit_pass_fail_pair(
    "sensor_read_to_transmission_within_interval",
    "sensor_interval_deviation",
    "transmission_complete",
    "is_interval_violation",
)}
"""


def emit_outputs(mode: str) -> str:
    lines = []

    if mode == "violations":
        for _, internal_name in CHECKS:
            lines.append(f"out violation_{internal_name}")

    elif mode == "checks":
        for public_name, _ in CHECKS:
            lines.append(f"out FAIL_{public_name}")
            lines.append(f"out PASS_{public_name}")

    else:
        raise ValueError(f"invalid mode: {mode}")

    return "\n".join(lines) + "\n"


def generate(
    target_interval_ticks: int,
    tolerance_ticks: int,
    mode: str = "violations",
) -> str:
    parts = [
        emit_header(),
        emit_latency_check(target_interval_ticks, tolerance_ticks),
        emit_outputs(mode),
    ]

    return "\n".join(parts)