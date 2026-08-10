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

def delay_tick_sum: Events[Int] =
  merge(if tick > 0 then default(last(delay_tick_sum, tick), 0) + tick else 0, 0)

def delay_block_check_event :=
  merge(state_id, merge(blocked_id, merge(delay_end_id, tick)))

def delay_ready_check_event :=
  merge(state_id, merge(ready_id, tick))

"""


def nested_merge(streams: list[str], fallback: str) -> str:
    if not streams:
        return fallback
    result = streams[-1]
    for stream in reversed(streams[:-1]):
        result = f"merge({stream}, {result})"
    return result


def emit_task(task_id: int) -> str:
    return f"""# ---------------- Delay task {task_id} ----------------
def delay_task_state_{task_id}: Events[Int] =
  merge(filter(state_new, state_id == {task_id}), {STATE_CREATED})

# Busy-delay model
def busy_start_ev_{task_id} :=
  filter(delay_busy_start_id, delay_busy_start_id == {task_id})

def busy_end_ev_{task_id} :=
  filter(delay_busy_end_id, delay_busy_end_id == {task_id})

def busy_start_time_{task_id}: Events[Int] =
  merge(time(busy_start_ev_{task_id}), -1)

def busy_end_time_{task_id}: Events[Int] =
  merge(time(busy_end_ev_{task_id}), -1)

def busy_start_tick_{task_id}: Events[Int] =
  merge(default(last(delay_tick_sum, busy_start_ev_{task_id}), 0), -1)

def busy_target_ticks_{task_id}: Events[Int] =
  merge(filter(delay_busy_start_ticks, delay_busy_start_id == {task_id}), 0)

def busy_active_at_state_{task_id} :=
  default(last(busy_start_time_{task_id}, state_id), -1) >
    default(last(busy_end_time_{task_id}, state_id), -1)

def viol_busy_delay_blocked_{task_id} :=
  filter(state_id,
    state_id == {task_id} && state_new == {STATE_BLOCKED} && busy_active_at_state_{task_id})

def viol_busy_delay_invalid_start_state_{task_id} :=
  filter(busy_start_ev_{task_id},
    default(last(delay_task_state_{task_id}, busy_start_ev_{task_id}), {STATE_CREATED}) !=
      {STATE_RUNNING})

def busy_elapsed_ticks_{task_id} :=
  default(last(delay_tick_sum, busy_end_ev_{task_id}), 0) -
    default(last(busy_start_tick_{task_id}, busy_end_ev_{task_id}), 0)

def viol_busy_delay_too_short_{task_id} :=
  filter(busy_end_ev_{task_id},
    busy_elapsed_ticks_{task_id} <
      default(last(busy_target_ticks_{task_id}, busy_end_ev_{task_id}), 0))

# Non-blocking delay model
def delay_start_ev_{task_id} :=
  filter(delay_start_id, delay_start_id == {task_id})

def delay_end_ev_{task_id} :=
  filter(delay_end_id, delay_end_id == {task_id})

def delay_start_time_{task_id}: Events[Int] =
  merge(time(delay_start_ev_{task_id}), -1)

def delay_end_time_{task_id}: Events[Int] =
  merge(time(delay_end_ev_{task_id}), -1)

def delay_start_tick_{task_id}: Events[Int] =
  merge(default(last(delay_tick_sum, delay_start_ev_{task_id}), 0), -1)

def delay_target_ticks_{task_id}: Events[Int] =
  merge(filter(delay_start_ticks, delay_start_id == {task_id}), 0)

def valid_delay_block_state_{task_id} :=
  filter(state_id,
    default(last(delay_start_time_{task_id}, state_id), -2) + 1 == time(state_id) &&
    state_id == {task_id} && state_old == {STATE_RUNNING} && state_new == {STATE_BLOCKED})

def valid_delay_block_state_time_{task_id}: Events[Int] =
  merge(time(valid_delay_block_state_{task_id}), -1)

def valid_delay_blocked_event_{task_id} :=
  filter(blocked_id,
    default(last(valid_delay_block_state_time_{task_id}, blocked_id), -2) + 1 ==
      time(blocked_id) && blocked_id == {task_id})

def valid_delay_blocked_event_time_{task_id}: Events[Int] =
  merge(time(valid_delay_blocked_event_{task_id}), -1)

def viol_delay_wrong_block_state_{task_id} :=
  filter(state_id,
    default(last(delay_start_time_{task_id}, state_id), -2) + 1 == time(state_id) &&
    (state_id != {task_id} || state_old != {STATE_RUNNING} || state_new != {STATE_BLOCKED}))

def viol_delay_wrong_blocked_event_{task_id} :=
  filter(blocked_id,
    default(last(valid_delay_block_state_time_{task_id}, blocked_id), -2) + 1 ==
      time(blocked_id) && blocked_id != {task_id})

def delay_block_state_pending_{task_id} :=
  default(last(delay_start_time_{task_id}, delay_block_check_event), -1) >
    default(last(valid_delay_block_state_time_{task_id}, delay_block_check_event), -1)

def delay_blocked_event_pending_{task_id} :=
  default(last(valid_delay_block_state_time_{task_id}, delay_block_check_event), -1) >
    default(last(valid_delay_blocked_event_time_{task_id}, delay_block_check_event), -1)

def viol_delay_missing_block_state_{task_id} :=
  filter(delay_block_check_event,
    time(delay_block_check_event) >
      default(last(delay_start_time_{task_id}, delay_block_check_event), -1) + 1 &&
    delay_block_state_pending_{task_id})

def viol_delay_missing_blocked_event_{task_id} :=
  filter(delay_block_check_event,
    time(delay_block_check_event) >
      default(last(valid_delay_block_state_time_{task_id}, delay_block_check_event), -1) + 1 &&
    delay_blocked_event_pending_{task_id})

def delay_elapsed_ticks_{task_id} :=
  default(last(delay_tick_sum, delay_end_ev_{task_id}), 0) -
    default(last(delay_start_tick_{task_id}, delay_end_ev_{task_id}), 0)

def viol_delay_too_short_{task_id} :=
  filter(delay_end_ev_{task_id},
    delay_elapsed_ticks_{task_id} <
      default(last(delay_target_ticks_{task_id}, delay_end_ev_{task_id}), 0))

def valid_delay_ready_state_{task_id} :=
  filter(state_id,
    default(last(delay_end_time_{task_id}, state_id), -2) + 1 == time(state_id) &&
    state_id == {task_id} && state_old == {STATE_BLOCKED} && state_new == {STATE_READY})

def valid_delay_ready_state_time_{task_id}: Events[Int] =
  merge(time(valid_delay_ready_state_{task_id}), -1)

def valid_delay_ready_event_{task_id} :=
  filter(ready_id,
    default(last(valid_delay_ready_state_time_{task_id}, ready_id), -2) + 1 ==
      time(ready_id) && ready_id == {task_id})

def valid_delay_ready_event_time_{task_id}: Events[Int] =
  merge(time(valid_delay_ready_event_{task_id}), -1)

def viol_delay_wrong_ready_state_{task_id} :=
  filter(state_id,
    default(last(delay_end_time_{task_id}, state_id), -2) + 1 == time(state_id) &&
    (state_id != {task_id} || state_old != {STATE_BLOCKED} || state_new != {STATE_READY}))

def viol_delay_wrong_ready_event_{task_id} :=
  filter(ready_id,
    default(last(valid_delay_ready_state_time_{task_id}, ready_id), -2) + 1 ==
      time(ready_id) && ready_id != {task_id})

def delay_ready_state_pending_{task_id} :=
  default(last(delay_end_time_{task_id}, delay_ready_check_event), -1) >
    default(last(valid_delay_ready_state_time_{task_id}, delay_ready_check_event), -1)

def delay_ready_event_pending_{task_id} :=
  default(last(valid_delay_ready_state_time_{task_id}, delay_ready_check_event), -1) >
    default(last(valid_delay_ready_event_time_{task_id}, delay_ready_check_event), -1)

def viol_delay_missing_ready_state_{task_id} :=
  filter(delay_ready_check_event,
    time(delay_ready_check_event) >
      default(last(delay_end_time_{task_id}, delay_ready_check_event), -1) + 1 &&
    delay_ready_state_pending_{task_id})

def viol_delay_missing_ready_event_{task_id} :=
  filter(delay_ready_check_event,
    time(delay_ready_check_event) >
      default(last(valid_delay_ready_state_time_{task_id}, delay_ready_check_event), -1) + 1 &&
    delay_ready_event_pending_{task_id})

"""


def emit_aggregates(max_tasks: int) -> str:
    def streams(prefix: str) -> list[str]:
        return [f"{prefix}_{task}" for task in range(max_tasks)]

    busy_blocked = nested_merge(streams("viol_busy_delay_blocked"), "filter(state_id, false)")
    busy_start = nested_merge(streams("viol_busy_delay_invalid_start_state"), "filter(delay_busy_start_id, false)")
    busy_short = nested_merge(streams("viol_busy_delay_too_short"), "filter(delay_busy_end_id, false)")
    delay_block = nested_merge(
        streams("viol_delay_wrong_block_state")
        + streams("viol_delay_wrong_blocked_event")
        + streams("viol_delay_missing_block_state")
        + streams("viol_delay_missing_blocked_event"),
        "filter(delay_block_check_event, false)",
    )
    delay_short = nested_merge(streams("viol_delay_too_short"), "filter(delay_end_id, false)")
    delay_ready = nested_merge(
        streams("viol_delay_wrong_ready_state")
        + streams("viol_delay_wrong_ready_event")
        + streams("viol_delay_missing_ready_state")
        + streams("viol_delay_missing_ready_event"),
        "filter(delay_ready_check_event, false)",
    )
    return f"""def violation_busy_delay_blocked :=
  {busy_blocked}

def violation_busy_delay_invalid_start_state :=
  {busy_start}

def violation_busy_delay_too_short :=
  {busy_short}

def violation_delay_not_blocked :=
  {delay_block}

def violation_delay_too_short :=
  {delay_short}

def violation_delay_invalid_unblock_state :=
  {delay_ready}

"""


def emit_outputs(mode: str) -> str:
    lines = []
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
                f"def PASS_{public} := filter({trigger}, " f"time({trigger}) != delay_last_failure_{internal})"
            )
            lines.append(f"out FAIL_{public}")
            lines.append(f"out PASS_{public}")
    else:
        raise ValueError(f"invalid mode: {mode}")
    return "\n".join(lines) + "\n"


def generate(max_tasks: int, mode: str = "violations") -> str:
    if max_tasks <= 0:
        raise ValueError("max_tasks must be greater than zero")
    parts = [emit_header()]
    for task_id in range(max_tasks):
        parts.append(emit_task(task_id))
    parts.append(emit_aggregates(max_tasks))
    parts.append(emit_outputs(mode))
    return "\n".join(parts)
