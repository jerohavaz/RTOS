#!/usr/bin/env python3

CHECKS = [
    (
        "transmission_interval_within_100ms",
        "transmission_interval_deviation",
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


def emit_transmission_interval_check(
    target_interval_ticks: int,
    tolerance_ticks: int,
) -> str:
    min_ticks = target_interval_ticks - tolerance_ticks
    max_ticks = target_interval_ticks + tolerance_ticks

    return f"""# ==================== Transmission Interval Check ====================
# Tick-Stand der AKTUELLEN Übertragung
def current_tx_tick: Events[Int] = last(tick_sum, transmission_complete)

# Tick-Stand der VORHERIGEN Übertragung (-1 bei der allerersten Übertragung)
def prev_tx_tick: Events[Int] = default(last(current_tx_tick, transmission_complete), -1)

# Vergangene Ticks seit der letzten Übertragung
def elapsed_ticks: Events[Int] = current_tx_tick - prev_tx_tick

# Prüft, ob bereits eine vorherige Übertragung für einen Vergleich existiert
def is_valid_measurement: Events[Bool] = prev_tx_tick != -1

# Bedingung für Intervallabweichung (Soll: {target_interval_ticks} Ticks, Erlaubt: [{min_ticks}, {max_ticks}])
def is_interval_violation: Events[Bool] =
  is_valid_measurement && (elapsed_ticks < {min_ticks} || elapsed_ticks > {max_ticks})

# Output der gemessenen Tick-Anzahl bei einer Intervall-Verletzung
def violation_transmission_interval_deviation :=
  filter(elapsed_ticks, is_interval_violation)

{emit_pass_fail_pair(
    "transmission_interval_within_100ms",
    "transmission_interval_deviation",
    "elapsed_ticks",
    "is_valid_measurement && is_interval_violation",
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
        emit_transmission_interval_check(target_interval_ticks, tolerance_ticks),
        emit_outputs(mode),
    ]

    return "\n".join(parts)