#!/usr/bin/env python3

STATE_CREATED = 0
STATE_READY = 1
STATE_RUNNING = 2
STATE_BLOCKED = 3

CHECKS = [
    ("busy_delay_never_blocks", "busy_delay_blocked"),
    ("busy_delay_remains_running", "busy_delay_interrupted_state"),
    ("busy_delay_duration_respected_short", "busy_delay_too_short"),
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


def build_nested_merge(streams: list[str], fallback: str = "filter(state_id, false)") -> str:
    if not streams:
        return fallback
    result = streams[0]
    for s in streams[1:]:
        result = f"merge({result}, {s})"
    return result


def emit_busy_delay_checks(max_tasks: int = 8, busy_tasks: int = 8) -> str:
    num_tasks = max_tasks

    lines = [
        "# --- Globaler Tick-Zähler (Globale Zeit) ---",
        """def current_tick_count: Events[Int] :=
  default(last(current_tick_count, tick), 0) + if tick >= 0 then tick else 0
""",
    ]

    block_violations = []
    too_short_violations = []

    for t in range(num_tasks):
        lines.append(f"""# --- Task {t} Verification ---
def start_ev_{t} := filter(delay_busy_start_id, delay_busy_start_id == {t})
def start_ticks_ev_{t} := filter(delay_busy_start_ticks, delay_busy_start_id == {t})
def end_ev_{t}   := filter(delay_busy_end_id, delay_busy_end_id == {t})

# Aktueller Tick-Stand zum Zeitpunkt des Starts
def start_tick_{t} := default(last(current_tick_count, start_ev_{t}), 0)
def target_ticks_{t} := start_ticks_ev_{t}

# Activity State Tracking
def busy_active_{t}: Events[Bool] := merge(
  if start_ev_{t} >= 0 then true else false,
  if end_ev_{t} >= 0 then false else true
)
def is_in_busy_{t} := default(last(busy_active_{t}, state_id), false)

# Tick-Stand bei Ende
def end_tick_{t} := current_tick_count
def start_tick_at_end_{t} := default(last(start_tick_{t}, end_ev_{t}), 0)
def target_ticks_at_end_{t} := default(last(target_ticks_{t}, end_ev_{t}), 0)

def elapsed_global_ticks_{t} := end_tick_{t} - start_tick_at_end_{t}

# Violation 1: Wechsel zu BLOCKED während busy delay
def viol_block_{t} :=
  filter(state_id, state_id == {t} && state_new == {STATE_BLOCKED} && is_in_busy_{t})

# Violation 2: Zu wenige globale Ticks beim Ende-Event verstrichen
def viol_too_short_{t} :=
  filter(end_ev_{t}, elapsed_global_ticks_{t} < target_ticks_at_end_{t})
""")

        block_violations.append(f"viol_block_{t}")
        too_short_violations.append(f"viol_too_short_{t}")

    merged_block = build_nested_merge(block_violations, "filter(state_id, false)")
    merged_too_short = build_nested_merge(too_short_violations, "filter(delay_busy_end_id, false)")

    lines.append(f"""# --- Zusammenführung aller Violations ---
def violation_busy_delay_blocked := {merged_block}

def busy_delay_illegal_block :=
  default(last(violation_busy_delay_blocked >= 0, state_id), false)

{emit_pass_fail_pair(
    "busy_delay_never_blocks",
    "busy_delay_blocked",
    "state_id >= 0",
    "busy_delay_illegal_block",
)}

def busy_delay_invalid_state := state_new != {STATE_RUNNING}

def violation_busy_delay_interrupted_state :=
  filter(delay_busy_start_id, busy_delay_invalid_state)

{emit_pass_fail_pair(
    "busy_delay_remains_running",
    "busy_delay_interrupted_state",
    "delay_busy_start_id >= 0",
    "busy_delay_invalid_state",
)}

def violation_busy_delay_too_short := {merged_too_short}

def busy_delay_ended_too_early :=
  default(last(violation_busy_delay_too_short >= 0, delay_busy_end_id), false)

{emit_pass_fail_pair(
    "busy_delay_duration_respected_short",
    "busy_delay_too_short",
    "delay_busy_end_id >= 0",
    "busy_delay_ended_too_early",
)}
""")

    return "\n".join(lines)


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
    max_tasks: int = 8,
    busy_tasks: int = 8,
    mode: str = "violations",
) -> str:
    parts = [
        emit_header(),
        emit_busy_delay_checks(max_tasks, busy_tasks),
        emit_outputs(mode),
    ]

    return "\n".join(parts)


if __name__ == "__main__":
    print(generate())