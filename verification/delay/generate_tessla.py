#!/usr/bin/env python3

STATE_CREATED = 0
STATE_READY = 1
STATE_RUNNING = 2
STATE_BLOCKED = 3

CHECKS = [
    ("busy_delay_never_blocks", "busy_delay_blocked"),
    ("busy_delay_remains_running", "busy_delay_interrupted_state"),
    ("busy_delay_duration_respected_short", "busy_delay_too_short"),
    ("busy_delay_duration_respected_long", "busy_delay_too_long"),
]


def emit_header() -> str:
    return """in delay_busy_start_id: Events[Int]
in delay_busy_start_ticks: Events[Int]
in delay_busy_end_id: Events[Int]

in state_id: Events[Int]
in state_old: Events[Int]
in state_new: Events[Int]

in running_id: Events[Int]
in running_prio: Events[Int]

in tick: Events[Int]

"""


def emit_pass_fail_pair(
    public_name: str,
    internal_name: str,
    trigger_expr: str,
    fail_condition: str,
) -> str:
    marker_name = f"{internal_name}_check_marker"

    return f"""def {marker_name} :=
  if {trigger_expr} then 1
  else 0

def FAIL_{public_name} :=
  filter({marker_name}, {fail_condition})

def PASS_{public_name} :=
  filter({marker_name}, {fail_condition} == false)

"""


def emit_busy_delay_checks() -> str:
    return f"""# Regel 1: Ein Busy Delay darf NIEMALS nach BLOCKED (3) wechseln
def busy_delay_illegal_block :=
  state_new == {STATE_BLOCKED}

def violation_busy_delay_blocked :=
  filter(state_id, busy_delay_illegal_block)

{emit_pass_fail_pair(
    "busy_delay_never_blocks",
    "busy_delay_blocked",
    "state_id >= 0",
    "busy_delay_illegal_block",
)}

# Regel 2: Task muss während des Busy Delays im Zustand RUNNING bleiben
def busy_delay_invalid_state :=
  state_new != {STATE_RUNNING}

def violation_busy_delay_interrupted_state :=
  filter(delay_busy_start_id, busy_delay_invalid_state)

{emit_pass_fail_pair(
    "busy_delay_remains_running",
    "busy_delay_interrupted_state",
    "delay_busy_start_id >= 0",
    "busy_delay_invalid_state",
)}

# --- Regel 3: Tick-basierte Dauerüberprüfung ---

# Laufender Gesamtzähler für Ticks
def tick_sum: Events[Int] =
  merge(
    if tick > 0 then default(last(tick_sum, tick), 0) + tick else 0,
    0
  )

# Speichert die geforderten Ziel-Ticks ab dem Start-Event
def target_ticks: Events[Int] =
  merge(delay_busy_start_ticks, default(last(target_ticks, delay_busy_start_ticks), 0))

# Speichert den Tick-Zählerstand exakt zum Zeitpunkt des Starts
def start_tick_snapshot: Events[Int] =
  merge(
    last(tick_sum, delay_busy_start_id),
    default(last(start_tick_snapshot, delay_busy_start_id), 0)
  )

# Berechnet die verstrichenen Ticks beim Beenden des Delays
def elapsed_ticks :=
  last(tick_sum, delay_busy_end_id) - last(start_tick_snapshot, delay_busy_end_id)

# Bedingung A: Zu früh beendet
def busy_delay_ended_too_early :=
  elapsed_ticks < last(target_ticks, delay_busy_end_id)

def violation_busy_delay_too_short :=
  filter(delay_busy_end_id, busy_delay_ended_too_early)

{emit_pass_fail_pair(
    "busy_delay_duration_respected_short",
    "busy_delay_too_short",
    "delay_busy_end_id >= 0",
    "busy_delay_ended_too_early",
)}

# Bedingung B: Zu spät beendet
def busy_delay_ended_too_late :=
  elapsed_ticks > last(target_ticks, delay_busy_end_id)

def violation_busy_delay_too_long :=
  filter(delay_busy_end_id, busy_delay_ended_too_late)

{emit_pass_fail_pair(
    "busy_delay_duration_respected_long",
    "busy_delay_too_long",
    "delay_busy_end_id >= 0",
    "busy_delay_ended_too_late",
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
    max_tasks: int,
    mode: str,
) -> str:
    parts = [
        emit_header(),
        emit_busy_delay_checks(),
        emit_outputs(mode),
    ]

    return "\n".join(parts)