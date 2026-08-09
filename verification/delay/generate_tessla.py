STATE_CREATED = 0
STATE_READY = 1
STATE_RUNNING = 2
STATE_BLOCKED = 3

CHECKS = [
    # --- Busy Delay Checks ---
    ("busy_delay_never_blocks", "busy_delay_blocked"),
    ("busy_delay_remains_running", "busy_delay_interrupted_state"),
    ("busy_delay_duration_respected_short", "busy_delay_too_short"),
    
    # --- Non-Blocking os_delay Checks ---
    ("non_blocking_delay_must_block", "delay_not_blocked"),
    ("non_blocking_delay_duration_respected", "delay_too_short"),
    ("non_blocking_delay_returns_to_ready", "delay_invalid_unblock_state"),
]


def emit_header() -> str:
    return """in delay_busy_start_id: Events[Int]
in delay_busy_start_ticks: Events[Int]
in delay_busy_end_id: Events[Int]

in delay_start_id: Events[Int]
in delay_start_ticks: Events[Int]
in delay_end_id: Events[Int]

in state_id: Events[Int]
in state_old: Events[Int]
in state_new: Events[Int]

in ready_id: Events[Int]
in ready_prio: Events[Int]

in running_id: Events[Int]
in running_prio: Events[Int]

in blocked_id: Events[Int]

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

    return f"""def {marker_name} :=\n  if {trigger_expr} then 1\n  else 0\n\ndef FAIL_{public_name} :=\n  filter({marker_name}, {fail_condition})\n\ndef PASS_{public_name} :=\n  filter({marker_name}, {fail_condition} == false)\n\n"""


def build_nested_merge(streams: list[str], fallback: str = "filter(state_id, false)") -> str:
    if not streams:
        return fallback
    result = streams[0]
    for s in streams[1:]:
        result = f"merge({result}, {s})"
    return result


def emit_per_task(task_id: int) -> str:
    return f"""# ==================== Task {task_id} Isolation ====================
# --- Busy Delay Streams ---
def busy_start_ev_{task_id} := filter(delay_busy_start_id, delay_busy_start_id == {task_id})
def busy_end_ev_{task_id}   := filter(delay_busy_end_id, delay_busy_end_id == {task_id})

def busy_last_start_tick_{task_id}: Events[Int] =
  merge(
    last(tick_sum, busy_start_ev_{task_id}),
    default(last(busy_last_start_tick_{task_id}, state_id), -1)
  )

def busy_last_end_tick_{task_id}: Events[Int] =
  merge(
    last(tick_sum, busy_end_ev_{task_id}),
    default(last(busy_last_end_tick_{task_id}, state_id), -1)
  )

def is_in_busy_{task_id} := busy_last_start_tick_{task_id} > busy_last_end_tick_{task_id} && busy_last_start_tick_{task_id} >= 0

def busy_target_ticks_{task_id}: Events[Int] =
  merge(
    filter(delay_busy_start_ticks, delay_busy_start_id == {task_id}),
    default(last(busy_target_ticks_{task_id}, busy_end_ev_{task_id}), 0)
  )

def busy_elapsed_ticks_{task_id} :=
  last(tick_sum, busy_end_ev_{task_id}) - last(busy_last_start_tick_{task_id}, busy_end_ev_{task_id})

# --- Busy Delay Violations ---
def viol_busy_delay_blocked_{task_id} :=
  filter(state_id, state_id == {task_id} && state_new == {STATE_BLOCKED} && is_in_busy_{task_id})

def viol_busy_delay_too_short_{task_id} :=
  filter(busy_end_ev_{task_id}, busy_elapsed_ticks_{task_id} < default(last(busy_target_ticks_{task_id}, busy_end_ev_{task_id}), 0))


# --- Non-Blocking os_delay Streams ---
def delay_start_ev_{task_id} := filter(delay_start_id, delay_start_id == {task_id})
def delay_end_ev_{task_id}   := filter(delay_end_id, delay_end_id == {task_id})

def delay_last_start_tick_{task_id}: Events[Int] =
  merge(
    last(tick_sum, delay_start_ev_{task_id}),
    default(last(delay_last_start_tick_{task_id}, state_id), -1)
  )

def delay_last_end_tick_{task_id}: Events[Int] =
  merge(
    last(tick_sum, delay_end_ev_{task_id}),
    default(last(delay_last_end_tick_{task_id}, state_id), -1)
  )

def delay_target_ticks_{task_id}: Events[Int] =
  merge(
    filter(delay_start_ticks, delay_start_id == {task_id}),
    default(last(delay_target_ticks_{task_id}, delay_end_ev_{task_id}), 0)
  )

def delay_elapsed_ticks_{task_id} :=
  last(tick_sum, delay_end_ev_{task_id}) - last(delay_last_start_tick_{task_id}, delay_end_ev_{task_id})

# Violations
def viol_delay_not_blocked_{task_id} :=
  filter(state_id, state_id == {task_id} && state_new != {STATE_BLOCKED} && default(last(delay_last_start_tick_{task_id}, state_id) > default(last(delay_last_end_tick_{task_id}, state_id), -1), false))

def viol_delay_too_short_{task_id} :=
  filter(delay_end_ev_{task_id}, delay_elapsed_ticks_{task_id} < default(last(delay_target_ticks_{task_id}, delay_end_ev_{task_id}), 0))

def viol_delay_invalid_unblock_state_{task_id} :=
  filter(state_id, state_id == {task_id} && state_old == {STATE_BLOCKED} && state_new != {STATE_READY})

"""


def emit_summary_checks(max_tasks: int) -> str:
    # Merging Busy Violations
    busy_block_violations = [f"viol_busy_delay_blocked_{t}" for t in range(max_tasks)]
    busy_too_short_violations = [f"viol_busy_delay_too_short_{t}" for t in range(max_tasks)]

    # Merging os_delay Violations
    delay_no_block_violations = [f"viol_delay_not_blocked_{t}" for t in range(max_tasks)]
    delay_too_short_violations = [f"viol_delay_too_short_{t}" for t in range(max_tasks)]
    delay_invalid_unblock_violations = [f"viol_delay_invalid_unblock_state_{t}" for t in range(max_tasks)]

    merged_busy_block = build_nested_merge(busy_block_violations, "filter(state_id, false)")
    merged_busy_too_short = build_nested_merge(busy_too_short_violations, "filter(delay_busy_end_id, false)")

    merged_delay_no_block = build_nested_merge(delay_no_block_violations, "filter(state_id, false)")
    merged_delay_too_short = build_nested_merge(delay_too_short_violations, "filter(delay_end_id, false)")
    merged_delay_invalid_unblock = build_nested_merge(delay_invalid_unblock_violations, "filter(state_id, false)")

    return f"""# --- Zusammenführung aller Violations ---

# 1. Busy Delay Checks
def violation_busy_delay_blocked := {merged_busy_block}

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

def violation_busy_delay_too_short := {merged_busy_too_short}

def busy_delay_ended_too_early :=
  default(last(violation_busy_delay_too_short >= 0, delay_busy_end_id), false)

{emit_pass_fail_pair(
    "busy_delay_duration_respected_short",
    "busy_delay_too_short",
    "delay_busy_end_id >= 0",
    "busy_delay_ended_too_early",
)}

# 2. Non-Blocking os_delay Checks
def violation_delay_not_blocked := {merged_delay_no_block}

def delay_not_blocked_condition :=
  default(last(violation_delay_not_blocked >= 0, state_id), false)

{emit_pass_fail_pair(
    "non_blocking_delay_must_block",
    "delay_not_blocked",
    "state_id >= 0",
    "delay_not_blocked_condition",
)}

def violation_delay_too_short := {merged_delay_too_short}

def delay_ended_too_early :=
  default(last(violation_delay_too_short >= 0, delay_end_id), false)

{emit_pass_fail_pair(
    "non_blocking_delay_duration_respected",
    "delay_too_short",
    "delay_end_id >= 0",
    "delay_ended_too_early",
)}

def violation_delay_invalid_unblock_state := {merged_delay_invalid_unblock}

def delay_invalid_unblock_condition :=
  default(last(violation_delay_invalid_unblock_state >= 0, state_id), false)

{emit_pass_fail_pair(
    "non_blocking_delay_returns_to_ready",
    "delay_invalid_unblock_state",
    "state_id >= 0",
    "delay_invalid_unblock_condition",
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