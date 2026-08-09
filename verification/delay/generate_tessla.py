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
    """Verschachtelt merge(), da TeSSLa merge nur genau 2 Argumente erlaubt."""
    if not streams:
        return fallback
    result = streams[0]
    for s in streams[1:]:
        result = f"merge({result}, {s})"
    return result


def emit_per_task(task_id: int) -> str:
    return f"""# ==================== Task {task_id} Isolation ====================
def start_ev_{task_id} := filter(delay_busy_start_id, delay_busy_start_id == {task_id})
def end_ev_{task_id}   := filter(delay_busy_end_id, delay_busy_end_id == {task_id})

# Speichert den Tick-Stand des letzten Starts von Task {task_id}
def last_start_tick_{task_id}: Events[Int] =
  merge(
    last(tick_sum, start_ev_{task_id}),
    default(last(last_start_tick_{task_id}, state_id), -1)
  )

# Speichert den Tick-Stand des letzten Endes von Task {task_id}
def last_end_tick_{task_id}: Events[Int] =
  merge(
    last(tick_sum, end_ev_{task_id}),
    default(last(last_end_tick_{task_id}, state_id), -1)
  )

# Task {task_id} ist im Busy Delay, wenn der letzte Start nach dem letzten Ende lag
def is_in_busy_{task_id} := last_start_tick_{task_id} > last_end_tick_{task_id} && last_start_tick_{task_id} >= 0

# geforderte Ticks
def target_ticks_{task_id}: Events[Int] =
  merge(
    filter(delay_busy_start_ticks, delay_busy_start_id == {task_id}),
    default(last(target_ticks_{task_id}, end_ev_{task_id}), 0)
  )

def elapsed_ticks_{task_id} :=
  last(tick_sum, end_ev_{task_id}) - last(last_start_tick_{task_id}, end_ev_{task_id})

# Violation 1: Task {task_id} wechselt nach BLOCKED (3), während er im Busy Delay ist
def viol_block_{task_id} :=
  filter(state_id, state_id == {task_id} && state_new == {STATE_BLOCKED} && is_in_busy_{task_id})

# Violation 2: Delay für Task {task_id} endet zu früh
def viol_too_short_{task_id} :=
  filter(end_ev_{task_id}, elapsed_ticks_{task_id} < default(last(target_ticks_{task_id}, end_ev_{task_id}), 0))

"""


def emit_summary_checks(max_tasks: int) -> str:
    block_violations = [f"viol_block_{t}" for t in range(max_tasks)]
    too_short_violations = [f"viol_too_short_{t}" for t in range(max_tasks)]

    merged_block = build_nested_merge(block_violations, "filter(state_id, false)")
    merged_too_short = build_nested_merge(too_short_violations, "filter(delay_busy_end_id, false)")

    return f"""# --- Zusammenführung aller Violations ---
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
    mode: str = "violations",
) -> str:
    parts = [emit_header()]

    for task_id in range(max_tasks):
        parts.append(emit_per_task(task_id))

    parts.append(emit_summary_checks(max_tasks))
    parts.append(emit_outputs(mode))

    return "\n".join(parts)
