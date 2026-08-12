"""Generate the delay TeSSLa verification monitor.

Author: Martin
Author: Jerome
"""

STATE_CREATED = 0
STATE_READY = 1
STATE_RUNNING = 2
STATE_BLOCKED = 3


CHECKS = [
    ("busy_delay_never_blocks", "busy_delay_blocked", "state_id"),
    ("busy_delay_starts_running", "busy_delay_invalid_start_state", "delay_busy_start_id"),
    ("busy_delay_duration_respected", "busy_delay_too_short", "delay_busy_end_id"),
    ("non_blocking_delay_blocks", "delay_not_blocked", "delay_block_check_event"),
    ("non_blocking_delay_duration_respected", "delay_too_short", "delay_end_id"),
    ("non_blocking_delay_returns_ready", "delay_invalid_unblock_state", "delay_ready_check_event"),
]


def emit_header() -> str:
    """Emit the delay monitor documentation and input declarations."""
    return """# Module: delay
# Purpose: Verify busy-wait and scheduler-based delay state and duration rules.
# Generator: delay/generate_tessla.py
# Author: Martin
# Author: Jerome

in delay_busy_start_id: Events[Int]
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

# Sum positive scheduler ticks.  This preserves the original duration model:
# delay durations are measured in accumulated tick values, not wall-clock time.
def delay_tick_sum: Events[Int] =
  merge(if tick > 0 then default(last(delay_tick_sum, tick), 0) + tick else 0, 0)

# These are the same trigger streams used by the original implementation for
# detecting missing protocol events after their one-timestamp deadline.
def delay_block_check_event :=
  merge(state_id, merge(blocked_id, merge(delay_end_id, tick)))

def delay_ready_check_event :=
  merge(state_id, merge(ready_id, tick))

"""


def emit_value_helpers(max_tasks: int) -> str:
    """Helpers that operate on plain values.

    They are called only through explicit slift*, so Map.fold never captures an
    Events[...] value.  This avoids TeSSLa's ambiguous higher-order lifting.
    """
    return f"""# True iff at least one valid task has an unsatisfied expectation
# whose one-timestamp deadline has already passed.  Invalid task IDs are
# ignored, matching the old per-task generator (which generated no monitor for
# IDs outside 0..max_tasks-1).
def delay_has_overdue_value(
  pending: Map[Int, Int],
  satisfied: Map[Int, Int],
  now: Int
): Bool =
  Map.fold(
    pending,
    false,
    (bad: Bool, task: Int, pending_time: Int) =>
      bad ||
      (task >= 0 &&
       task < {max_tasks} &&
       now > pending_time + 1 &&
       pending_time > Map.getOrElse(satisfied, task, -1))
  )

"""


def emit_task_state_model() -> str:
    """Maintain the last state of every task in one runtime map."""
    return """# ---------------- Shared task state ----------------
#
# The old generator emitted one delay_task_state_N stream per task.  This map
# is the equivalent runtime representation and is independent of max_tasks.
def delay_old_task_states_at_state :=
  default(last(delay_task_states, state_id), Map.empty[Int, Int])

def delay_task_states: Events[Map[Int, Int]] =
  merge(
    Map.add(delay_old_task_states_at_state, state_id, state_new),
    Map.empty[Int, Int]
  )

"""


def emit_busy_model(max_tasks: int) -> str:
    """Emit busy-delay state, duration, and running-state checks."""
    return f"""# ---------------- Busy delay ----------------
#
# Keep the latest busy start/end timestamp, start tick and requested duration
# per task.  The number of TeSSLa streams is fixed; only the runtime maps grow.

def delay_old_busy_start_times_at_start :=
  default(last(delay_busy_start_times, delay_busy_start_id), Map.empty[Int, Int])

def delay_busy_start_times: Events[Map[Int, Int]] =
  merge(
    Map.add(
      delay_old_busy_start_times_at_start,
      delay_busy_start_id,
      time(delay_busy_start_id)
    ),
    Map.empty[Int, Int]
  )

def delay_old_busy_end_times_at_end :=
  default(last(delay_busy_end_times, delay_busy_end_id), Map.empty[Int, Int])

def delay_busy_end_times: Events[Map[Int, Int]] =
  merge(
    Map.add(
      delay_old_busy_end_times_at_end,
      delay_busy_end_id,
      time(delay_busy_end_id)
    ),
    Map.empty[Int, Int]
  )

# Snapshot the accumulated tick count strictly before each busy start, matching
# last(delay_tick_sum, busy_start_ev_N) from the old per-task monitor.
def delay_busy_start_tick_value :=
  default(last(delay_tick_sum, delay_busy_start_id), 0)

def delay_old_busy_start_ticks_at_start :=
  default(last(delay_busy_start_ticks_by_task, delay_busy_start_id), Map.empty[Int, Int])

def delay_busy_start_ticks_by_task: Events[Map[Int, Int]] =
  merge(
    Map.add(
      delay_old_busy_start_ticks_at_start,
      delay_busy_start_id,
      delay_busy_start_tick_value
    ),
    Map.empty[Int, Int]
  )

# delay_busy_start_ticks is a separate input stream.  Associate each duration
# event with the current/last busy-start ID exactly as the old filter-based
# implementation did.
def delay_busy_target_task :=
  on(delay_busy_start_ticks, delay_busy_start_id)

def delay_old_busy_targets_at_ticks :=
  default(last(delay_busy_targets, delay_busy_start_ticks), Map.empty[Int, Int])

def delay_busy_targets: Events[Map[Int, Int]] =
  merge(
    Map.add(
      delay_old_busy_targets_at_ticks,
      delay_busy_target_task,
      delay_busy_start_ticks
    ),
    Map.empty[Int, Int]
  )

# busy_delay_never_blocks
def delay_busy_start_times_at_state :=
  default(last(delay_busy_start_times, state_id), Map.empty[Int, Int])

def delay_busy_end_times_at_state :=
  default(last(delay_busy_end_times, state_id), Map.empty[Int, Int])

def delay_busy_state_id_valid :=
  state_id >= 0 && state_id < {max_tasks}

def delay_busy_active_at_state :=
  Map.getOrElse(delay_busy_start_times_at_state, state_id, -1) >
  Map.getOrElse(delay_busy_end_times_at_state, state_id, -1)

def violation_busy_delay_blocked :=
  filter(
    state_id,
    delay_busy_state_id_valid &&
    state_new == {STATE_BLOCKED} &&
    delay_busy_active_at_state
  )

# busy_delay_starts_running
def delay_states_at_busy_start :=
  default(last(delay_task_states, delay_busy_start_id), Map.empty[Int, Int])

def delay_busy_start_id_valid :=
  delay_busy_start_id >= 0 && delay_busy_start_id < {max_tasks}

def delay_busy_invalid_start_state :=
  delay_busy_start_id_valid &&
  Map.getOrElse(
    delay_states_at_busy_start,
    delay_busy_start_id,
    {STATE_CREATED}
  ) != {STATE_RUNNING}

def violation_busy_delay_invalid_start_state :=
  filter(delay_busy_start_id, delay_busy_invalid_start_state)

# busy_delay_duration_respected
def delay_tick_sum_at_busy_end :=
  default(last(delay_tick_sum, delay_busy_end_id), 0)

def delay_busy_start_ticks_at_end :=
  default(
    last(delay_busy_start_ticks_by_task, delay_busy_end_id),
    Map.empty[Int, Int]
  )

def delay_busy_targets_at_end :=
  default(last(delay_busy_targets, delay_busy_end_id), Map.empty[Int, Int])

def delay_busy_end_id_valid :=
  delay_busy_end_id >= 0 && delay_busy_end_id < {max_tasks}

def delay_busy_elapsed_ticks :=
  delay_tick_sum_at_busy_end -
  Map.getOrElse(delay_busy_start_ticks_at_end, delay_busy_end_id, 0)

def delay_busy_target_at_end :=
  Map.getOrElse(delay_busy_targets_at_end, delay_busy_end_id, 0)

def delay_busy_too_short :=
  delay_busy_end_id_valid &&
  delay_busy_elapsed_ticks < delay_busy_target_at_end

def violation_busy_delay_too_short :=
  filter(delay_busy_end_id, delay_busy_too_short)

"""


def emit_nonblocking_history() -> str:
    """Maps that replace all per-task start/end/valid-event timestamp streams."""
    return """# ---------------- Non-blocking delay history ----------------

def delay_old_start_times_at_start :=
  default(last(delay_start_times, delay_start_id), Map.empty[Int, Int])

def delay_start_times: Events[Map[Int, Int]] =
  merge(
    Map.add(delay_old_start_times_at_start, delay_start_id, time(delay_start_id)),
    Map.empty[Int, Int]
  )

def delay_start_tick_value :=
  default(last(delay_tick_sum, delay_start_id), 0)

def delay_old_start_ticks_at_start :=
  default(last(delay_start_ticks_by_task, delay_start_id), Map.empty[Int, Int])

def delay_start_ticks_by_task: Events[Map[Int, Int]] =
  merge(
    Map.add(
      delay_old_start_ticks_at_start,
      delay_start_id,
      delay_start_tick_value
    ),
    Map.empty[Int, Int]
  )

def delay_target_task :=
  on(delay_start_ticks, delay_start_id)

def delay_old_targets_at_ticks :=
  default(last(delay_targets, delay_start_ticks), Map.empty[Int, Int])

def delay_targets: Events[Map[Int, Int]] =
  merge(
    Map.add(
      delay_old_targets_at_ticks,
      delay_target_task,
      delay_start_ticks
    ),
    Map.empty[Int, Int]
  )

def delay_old_end_times_at_end :=
  default(last(delay_end_times, delay_end_id), Map.empty[Int, Int])

def delay_end_times: Events[Map[Int, Int]] =
  merge(
    Map.add(delay_old_end_times_at_end, delay_end_id, time(delay_end_id)),
    Map.empty[Int, Int]
  )

"""


def emit_block_protocol(max_tasks: int) -> str:
    """Emit the blocking-delay transition and BLOCKED-event protocol."""
    return f"""# ---------------- delay_start -> BLOCKED protocol ----------------
#
# Original semantics:
#   delay_start(task) at t
#   STATE task RUNNING -> BLOCKED at t+1
#   BLOCKED(task) at t+2
#
# Wrong events are reported at their exact expected timestamp.  Missing events
# are reported once time has advanced beyond the expected timestamp.

def delay_previous_start_id_at_state :=
  default(last(delay_start_id, state_id), -1)

def delay_previous_start_time_at_state :=
  default(last(time(delay_start_id), state_id), -2)

def delay_previous_start_id_valid :=
  delay_previous_start_id_at_state >= 0 &&
  delay_previous_start_id_at_state < {max_tasks}

def delay_block_state_expected_now :=
  delay_previous_start_id_valid &&
  delay_previous_start_time_at_state + 1 == time(state_id)

def valid_delay_block_state :=
  filter(
    state_id,
    delay_block_state_expected_now &&
    state_id == delay_previous_start_id_at_state &&
    state_old == {STATE_RUNNING} &&
    state_new == {STATE_BLOCKED}
  )

def delay_wrong_block_state :=
  delay_block_state_expected_now &&
  (
    state_id != delay_previous_start_id_at_state ||
    state_old != {STATE_RUNNING} ||
    state_new != {STATE_BLOCKED}
  )

def viol_delay_wrong_block_state :=
  filter(state_id, delay_wrong_block_state)

# Per-task timestamp of the latest valid RUNNING -> BLOCKED state transition.
def delay_old_valid_block_state_times_at_valid :=
  default(
    last(delay_valid_block_state_times, valid_delay_block_state),
    Map.empty[Int, Int]
  )

def delay_valid_block_state_times: Events[Map[Int, Int]] =
  merge(
    Map.add(
      delay_old_valid_block_state_times_at_valid,
      valid_delay_block_state,
      time(valid_delay_block_state)
    ),
    Map.empty[Int, Int]
  )

# A BLOCKED event must immediately follow the valid state transition.
def delay_previous_block_state_id_at_blocked :=
  default(last(valid_delay_block_state, blocked_id), -1)

def delay_previous_block_state_time_at_blocked :=
  default(last(time(valid_delay_block_state), blocked_id), -2)

def delay_previous_block_state_id_valid :=
  delay_previous_block_state_id_at_blocked >= 0 &&
  delay_previous_block_state_id_at_blocked < {max_tasks}

def delay_blocked_event_expected_now :=
  delay_previous_block_state_id_valid &&
  delay_previous_block_state_time_at_blocked + 1 == time(blocked_id)

def valid_delay_blocked_event :=
  filter(
    blocked_id,
    delay_blocked_event_expected_now &&
    blocked_id == delay_previous_block_state_id_at_blocked
  )

def delay_wrong_blocked_event :=
  delay_blocked_event_expected_now &&
  blocked_id != delay_previous_block_state_id_at_blocked

def viol_delay_wrong_blocked_event :=
  filter(blocked_id, delay_wrong_blocked_event)

def delay_old_valid_blocked_event_times_at_valid :=
  default(
    last(delay_valid_blocked_event_times, valid_delay_blocked_event),
    Map.empty[Int, Int]
  )

def delay_valid_blocked_event_times: Events[Map[Int, Int]] =
  merge(
    Map.add(
      delay_old_valid_blocked_event_times_at_valid,
      valid_delay_blocked_event,
      time(valid_delay_blocked_event)
    ),
    Map.empty[Int, Int]
  )

# Missing block-state expectations.
def delay_start_times_at_block_check :=
  default(last(delay_start_times, delay_block_check_event), Map.empty[Int, Int])

def delay_valid_block_state_times_at_block_check :=
  default(
    last(delay_valid_block_state_times, delay_block_check_event),
    Map.empty[Int, Int]
  )

def delay_missing_block_state :=
  slift3(
    delay_start_times_at_block_check,
    delay_valid_block_state_times_at_block_check,
    time(delay_block_check_event),
    (
      pending: Map[Int, Int],
      satisfied: Map[Int, Int],
      now: Int
    ) => delay_has_overdue_value(pending, satisfied, now)
  )

def viol_delay_missing_block_state :=
  filter(delay_block_check_event, delay_missing_block_state)

# Missing BLOCKED-event expectations.
def delay_valid_blocked_event_times_at_block_check :=
  default(
    last(delay_valid_blocked_event_times, delay_block_check_event),
    Map.empty[Int, Int]
  )

def delay_missing_blocked_event :=
  slift3(
    delay_valid_block_state_times_at_block_check,
    delay_valid_blocked_event_times_at_block_check,
    time(delay_block_check_event),
    (
      pending: Map[Int, Int],
      satisfied: Map[Int, Int],
      now: Int
    ) => delay_has_overdue_value(pending, satisfied, now)
  )

def viol_delay_missing_blocked_event :=
  filter(delay_block_check_event, delay_missing_blocked_event)

def violation_delay_not_blocked :=
  merge(
    viol_delay_wrong_block_state,
    merge(
      viol_delay_wrong_blocked_event,
      merge(
        viol_delay_missing_block_state,
        viol_delay_missing_blocked_event
      )
    )
  )

"""


def emit_duration_check(max_tasks: int) -> str:
    """Emit the minimum-duration check for scheduler-based delays."""
    return f"""# ---------------- Non-blocking duration ----------------

def delay_tick_sum_at_end :=
  default(last(delay_tick_sum, delay_end_id), 0)

def delay_start_ticks_at_end :=
  default(last(delay_start_ticks_by_task, delay_end_id), Map.empty[Int, Int])

def delay_targets_at_end :=
  default(last(delay_targets, delay_end_id), Map.empty[Int, Int])

def delay_end_id_valid :=
  delay_end_id >= 0 && delay_end_id < {max_tasks}

def delay_elapsed_ticks :=
  delay_tick_sum_at_end -
  Map.getOrElse(delay_start_ticks_at_end, delay_end_id, 0)

def delay_target_at_end :=
  Map.getOrElse(delay_targets_at_end, delay_end_id, 0)

def delay_too_short :=
  delay_end_id_valid &&
  delay_elapsed_ticks < delay_target_at_end

def violation_delay_too_short :=
  filter(delay_end_id, delay_too_short)

"""


def emit_ready_protocol(max_tasks: int) -> str:
    """Emit the delay-completion READY transition and event protocol."""
    return f"""# ---------------- delay_end -> READY protocol ----------------
#
# Original semantics:
#   delay_end(task) at t
#   STATE task BLOCKED -> READY at t+1
#   READY(task) at t+2

def delay_previous_end_id_at_state :=
  default(last(delay_end_id, state_id), -1)

def delay_previous_end_time_at_state :=
  default(last(time(delay_end_id), state_id), -2)

def delay_previous_end_id_valid :=
  delay_previous_end_id_at_state >= 0 &&
  delay_previous_end_id_at_state < {max_tasks}

def delay_ready_state_expected_now :=
  delay_previous_end_id_valid &&
  delay_previous_end_time_at_state + 1 == time(state_id)

def valid_delay_ready_state :=
  filter(
    state_id,
    delay_ready_state_expected_now &&
    state_id == delay_previous_end_id_at_state &&
    state_old == {STATE_BLOCKED} &&
    state_new == {STATE_READY}
  )

def delay_wrong_ready_state :=
  delay_ready_state_expected_now &&
  (
    state_id != delay_previous_end_id_at_state ||
    state_old != {STATE_BLOCKED} ||
    state_new != {STATE_READY}
  )

def viol_delay_wrong_ready_state :=
  filter(state_id, delay_wrong_ready_state)

def delay_old_valid_ready_state_times_at_valid :=
  default(
    last(delay_valid_ready_state_times, valid_delay_ready_state),
    Map.empty[Int, Int]
  )

def delay_valid_ready_state_times: Events[Map[Int, Int]] =
  merge(
    Map.add(
      delay_old_valid_ready_state_times_at_valid,
      valid_delay_ready_state,
      time(valid_delay_ready_state)
    ),
    Map.empty[Int, Int]
  )

# READY(task) must immediately follow the valid BLOCKED -> READY transition.
def delay_previous_ready_state_id_at_ready :=
  default(last(valid_delay_ready_state, ready_id), -1)

def delay_previous_ready_state_time_at_ready :=
  default(last(time(valid_delay_ready_state), ready_id), -2)

def delay_previous_ready_state_id_valid :=
  delay_previous_ready_state_id_at_ready >= 0 &&
  delay_previous_ready_state_id_at_ready < {max_tasks}

def delay_ready_event_expected_now :=
  delay_previous_ready_state_id_valid &&
  delay_previous_ready_state_time_at_ready + 1 == time(ready_id)

def valid_delay_ready_event :=
  filter(
    ready_id,
    delay_ready_event_expected_now &&
    ready_id == delay_previous_ready_state_id_at_ready
  )

def delay_wrong_ready_event :=
  delay_ready_event_expected_now &&
  ready_id != delay_previous_ready_state_id_at_ready

def viol_delay_wrong_ready_event :=
  filter(ready_id, delay_wrong_ready_event)

def delay_old_valid_ready_event_times_at_valid :=
  default(
    last(delay_valid_ready_event_times, valid_delay_ready_event),
    Map.empty[Int, Int]
  )

def delay_valid_ready_event_times: Events[Map[Int, Int]] =
  merge(
    Map.add(
      delay_old_valid_ready_event_times_at_valid,
      valid_delay_ready_event,
      time(valid_delay_ready_event)
    ),
    Map.empty[Int, Int]
  )

# Missing READY-state expectations.
def delay_end_times_at_ready_check :=
  default(last(delay_end_times, delay_ready_check_event), Map.empty[Int, Int])

def delay_valid_ready_state_times_at_ready_check :=
  default(
    last(delay_valid_ready_state_times, delay_ready_check_event),
    Map.empty[Int, Int]
  )

def delay_missing_ready_state :=
  slift3(
    delay_end_times_at_ready_check,
    delay_valid_ready_state_times_at_ready_check,
    time(delay_ready_check_event),
    (
      pending: Map[Int, Int],
      satisfied: Map[Int, Int],
      now: Int
    ) => delay_has_overdue_value(pending, satisfied, now)
  )

def viol_delay_missing_ready_state :=
  filter(delay_ready_check_event, delay_missing_ready_state)

# Missing READY-event expectations.
def delay_valid_ready_event_times_at_ready_check :=
  default(
    last(delay_valid_ready_event_times, delay_ready_check_event),
    Map.empty[Int, Int]
  )

def delay_missing_ready_event :=
  slift3(
    delay_valid_ready_state_times_at_ready_check,
    delay_valid_ready_event_times_at_ready_check,
    time(delay_ready_check_event),
    (
      pending: Map[Int, Int],
      satisfied: Map[Int, Int],
      now: Int
    ) => delay_has_overdue_value(pending, satisfied, now)
  )

def viol_delay_missing_ready_event :=
  filter(delay_ready_check_event, delay_missing_ready_event)

def violation_delay_invalid_unblock_state :=
  merge(
    viol_delay_wrong_ready_state,
    merge(
      viol_delay_wrong_ready_event,
      merge(
        viol_delay_missing_ready_state,
        viol_delay_missing_ready_event
      )
    )
  )

"""


def emit_outputs(mode: str) -> str:
    """Emit violation streams or paired PASS/FAIL output declarations."""
    # Preserve the original public output names and PASS/FAIL behavior so the
    # existing config.py/tests continue to work unchanged.
    lines: list[str] = []

    if mode == "violations":
        for _, internal, _ in CHECKS:
            lines.append(f"out violation_{internal}")

    elif mode == "checks":
        for public, internal, trigger in CHECKS:
            lines.append(f"def FAIL_{public} := violation_{internal}")
            lines.append(
                f"def delay_last_failure_{internal} := "
                f"merge(time(violation_{internal}), "
                f"default(last(time(violation_{internal}), {trigger}), -1))"
            )
            lines.append(
                f"def PASS_{public} := filter(" f"{trigger}, time({trigger}) != delay_last_failure_{internal})"
            )
            lines.append(f"out FAIL_{public}")
            lines.append(f"out PASS_{public}")

    else:
        raise ValueError(f"invalid mode: {mode}")

    return "\n".join(lines) + "\n"


def generate(max_tasks: int, mode: str = "violations") -> str:
    """Return a delay monitor for the configured task-ID range and output mode."""
    if max_tasks <= 0:
        raise ValueError("max_tasks must be greater than zero")

    parts = [
        emit_header(),
        emit_value_helpers(max_tasks),
        emit_task_state_model(),
        emit_busy_model(max_tasks),
        emit_nonblocking_history(),
        emit_block_protocol(max_tasks),
        emit_duration_check(max_tasks),
        emit_ready_protocol(max_tasks),
        emit_outputs(mode),
    ]

    return "\n".join(parts)
