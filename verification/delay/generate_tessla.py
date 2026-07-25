#!/usr/bin/env python3

STATE_CREATED = 0
STATE_READY = 1
STATE_RUNNING = 2
STATE_BLOCKED = 3

CHECKS = [
    ("busy_delay_never_blocks", "busy_delay_blocked"),
    ("busy_delay_remains_running", "busy_delay_interrupted_state"),
]


def emit_header() -> str:
    return """in delay_busy_start_id: Events[Int]
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
    return f"""# Signalisiert, ob sich eine Task in einer Busy-Delay-Phase befindet
def is_busy_delaying :=
  merge(
    default(delay_busy_start_id, -1) != -1,
    if delay_busy_end_id >= 0 then false else true
  )

# Regel 1: Ein Busy Delay darf NIEMALS eine Transition nach BLOCKED (3) bewirken
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

# Regel 2: Während eines Busy Delays darf der Status der Task nicht von RUNNING abweichen
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