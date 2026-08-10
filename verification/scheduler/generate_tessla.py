STATE_CREATED = 0
STATE_READY = 1
STATE_RUNNING = 2
STATE_BLOCKED = 3


CHECKS = [
    ("task_ids_in_range", "task_id_out_of_range"),
    ("valid_state_transition", "invalid_state_transition"),
    ("state_history_continuous", "state_discontinuity"),
    ("ready_event_matches_state", "ready_event_inconsistent"),
    ("running_event_matches_state", "running_event_inconsistent"),
    ("blocked_event_matches_state", "blocked_event_inconsistent"),
    ("task_priority_consistent", "task_priority_inconsistent"),
    ("blocked_task_not_running", "blocked_running"),
    ("idle_only_when_no_task_ready", "idle_while_ready"),
    ("highest_priority_runs", "priority"),
    ("round_robin_fifo_respected", "round_robin"),
    ("one_tick_rotation_respected", "tick_rotation"),
]


def emit_header() -> str:
    return """in task_create_id: Events[Int]
in task_create_prio: Events[Int]

in state_id: Events[Int]
in state_old: Events[Int]
in state_new: Events[Int]

in ready_id: Events[Int]
in ready_prio: Events[Int]

in running_id: Events[Int]
in running_prio: Events[Int]

in blocked_id: Events[Int]

in idle: Events[Bool]

in tick: Events[Int]

"""


def or_terms(terms: list[str]) -> str:
    if not terms:
        return "false"
    return " ||\n  ".join(terms)


def nested_merge(streams: list[str], fallback: str) -> str:
    if not streams:
        return fallback
    result = streams[-1]
    for stream in reversed(streams[:-1]):
        result = f"merge({stream}, {result})"
    return result


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


def emit_event_model() -> str:
    return """def scheduler_task_id_event :=
  merge(task_create_id, merge(state_id, merge(ready_id, merge(running_id, blocked_id))))

def scheduler_ready_sequence: Events[Int] =
  merge(default(last(scheduler_ready_sequence, ready_id), 0) + 1, 0)

def scheduler_running_decisions: Events[Int] =
  merge(default(last(scheduler_running_decisions, running_id), 0) + 1, 0)

def scheduler_current_running: Events[Int] =
  merge(running_id, -1)

def scheduler_current_running_prio: Events[Int] =
  merge(running_prio, -1)

"""


def emit_per_task(task_id: int) -> str:
    return f"""def task_state_{task_id}: Events[Int] =
  merge(filter(state_new, state_id == {task_id}), {STATE_CREATED})

def task_created_priority_{task_id}: Events[Int] =
  merge(filter(task_create_prio, task_create_id == {task_id}), -1)

def task_ready_priority_{task_id}: Events[Int] =
  merge(filter(ready_prio, ready_id == {task_id}), -1)

def task_ready_order_{task_id}: Events[Int] =
  merge(filter(scheduler_ready_sequence, ready_id == {task_id}), -1)

"""


def emit_task_id_bounds(max_tasks: int) -> str:
    streams = []
    for name in ("task_create_id", "state_id", "ready_id", "running_id", "blocked_id"):
        stream = f"scheduler_invalid_{name}"
        streams.append(stream)
    definitions = []
    for name, stream in zip(("task_create_id", "state_id", "ready_id", "running_id", "blocked_id"), streams):
        definitions.append(f"def {stream} :=\n  filter({name}, {name} < 0 || {name} >= {max_tasks})\n")
    return "\n".join(definitions) + f"""
def violation_task_id_out_of_range :=
  {nested_merge(streams, 'filter(scheduler_task_id_event, false)')}

{emit_pass_fail_pair(
    'task_ids_in_range',
    'task_id_out_of_range',
    'scheduler_task_id_event >= 0',
    f'scheduler_task_id_event < 0 || scheduler_task_id_event >= {max_tasks}',
)}"""


def emit_state_checks(max_tasks: int) -> str:
    continuity = []
    for task_id in range(max_tasks):
        continuity.append(
            f"(state_id == {task_id} && state_old != "
            f"default(last(task_state_{task_id}, state_id), {STATE_CREATED}))"
        )
    return f"""def valid_state_transition :=
  if state_old == {STATE_CREATED} then state_new == {STATE_READY}
  else if state_old == {STATE_READY} then state_new == {STATE_RUNNING} || state_new == {STATE_BLOCKED}
  else if state_old == {STATE_RUNNING} then state_new == {STATE_READY} || state_new == {STATE_BLOCKED}
  else if state_old == {STATE_BLOCKED} then state_new == {STATE_READY}
  else false

def violation_invalid_state_transition :=
  filter(state_id, valid_state_transition == false)

def scheduler_state_discontinuity_bad :=
  {or_terms(continuity)}

def violation_state_discontinuity :=
  filter(state_id, scheduler_state_discontinuity_bad)

{emit_pass_fail_pair(
    'valid_state_transition',
    'invalid_state_transition',
    'state_id >= 0',
    'valid_state_transition == false',
)}{emit_pass_fail_pair(
    'state_history_continuous',
    'state_discontinuity',
    'state_id >= 0',
    'scheduler_state_discontinuity_bad',
)}"""


def emit_event_consistency(max_tasks: int) -> str:
    ready_terms = []
    running_terms = []
    blocked_terms = []
    priority_terms = []

    for task_id in range(max_tasks):
        ready_terms.append(
            f"(ready_id == {task_id} && ("
            f"default(last(task_state_{task_id}, ready_id), {STATE_CREATED}) != {STATE_READY} || "
            f"default(last(state_id, ready_id), -1) != {task_id} || "
            f"default(last(state_new, ready_id), -1) != {STATE_READY} || "
            f"default(last(time(state_id), ready_id), -2) + 1 != time(ready_id)))"
        )
        running_terms.append(
            f"(running_id == {task_id} && ("
            f"default(last(task_state_{task_id}, running_id), {STATE_CREATED}) != {STATE_RUNNING} || "
            f"default(last(state_id, running_id), -1) != {task_id} || "
            f"default(last(state_new, running_id), -1) != {STATE_RUNNING} || "
            f"default(last(time(state_id), running_id), -2) + 1 != time(running_id) || "
            f"default(last(task_ready_order_{task_id}, running_id), -1) < 0))"
        )
        blocked_terms.append(
            f"(blocked_id == {task_id} && ("
            f"default(last(task_state_{task_id}, blocked_id), {STATE_CREATED}) != {STATE_BLOCKED} || "
            f"default(last(state_id, blocked_id), -1) != {task_id} || "
            f"default(last(state_new, blocked_id), -1) != {STATE_BLOCKED} || "
            f"default(last(time(state_id), blocked_id), -2) + 1 != time(blocked_id)))"
        )
        priority_terms.append(
            f"(ready_id == {task_id} && "
            f"default(last(task_created_priority_{task_id}, ready_id), -1) >= 0 && "
            f"ready_prio != default(last(task_created_priority_{task_id}, ready_id), -1))"
        )
        priority_terms.append(
            f"(running_id == {task_id} && "
            f"default(last(task_created_priority_{task_id}, running_id), -1) >= 0 && "
            f"running_prio != default(last(task_created_priority_{task_id}, running_id), -1))"
        )

    return f"""def ready_event_inconsistent :=
  {or_terms(ready_terms)}

def running_event_inconsistent :=
  {or_terms(running_terms)}

def blocked_event_inconsistent :=
  {or_terms(blocked_terms)}

def scheduler_task_priority_inconsistent :=
  {or_terms(priority_terms)}

def violation_ready_event_inconsistent :=
  filter(ready_id, ready_event_inconsistent)

def violation_running_event_inconsistent :=
  filter(running_id, running_event_inconsistent)

def violation_blocked_event_inconsistent :=
  filter(blocked_id, blocked_event_inconsistent)

def scheduler_priority_event :=
  merge(ready_id, running_id)

def violation_task_priority_inconsistent :=
  filter(scheduler_priority_event, scheduler_task_priority_inconsistent)

{emit_pass_fail_pair(
    'ready_event_matches_state',
    'ready_event_inconsistent',
    'ready_id >= 0',
    'ready_event_inconsistent',
)}{emit_pass_fail_pair(
    'running_event_matches_state',
    'running_event_inconsistent',
    'running_id >= 0',
    'running_event_inconsistent',
)}{emit_pass_fail_pair(
    'blocked_event_matches_state',
    'blocked_event_inconsistent',
    'blocked_id >= 0',
    'blocked_event_inconsistent',
)}{emit_pass_fail_pair(
    'task_priority_consistent',
    'task_priority_inconsistent',
    'scheduler_priority_event >= 0',
    'scheduler_task_priority_inconsistent',
)}"""


def emit_blocked_check(max_tasks: int) -> str:
    terms = [
        f"(running_id == {task_id} && "
        f"default(last(task_state_{task_id}, running_id), {STATE_CREATED}) == {STATE_BLOCKED})"
        for task_id in range(max_tasks)
    ]
    return f"""def blocked_task_running :=
  {or_terms(terms)}

def violation_blocked_running :=
  filter(running_id, blocked_task_running)

{emit_pass_fail_pair(
    'blocked_task_not_running',
    'blocked_running',
    'running_id >= 0',
    'blocked_task_running',
)}"""


def emit_idle_check(max_tasks: int) -> str:
    ready = [
        f"default(last(task_state_{task_id}, idle), {STATE_CREATED}) == {STATE_READY}" for task_id in range(max_tasks)
    ]
    return f"""def any_task_ready_at_idle :=
  {or_terms(ready)}

def idle_bad :=
  idle && any_task_ready_at_idle

def violation_idle_while_ready :=
  filter(idle, idle_bad)

{emit_pass_fail_pair(
    'idle_only_when_no_task_ready',
    'idle_while_ready',
    'idle',
    'idle_bad',
)}"""


def emit_priority_and_fifo(max_tasks: int) -> str:
    priority_terms = []
    fifo_terms = []

    for running_task in range(max_tasks):
        chosen_order = f"default(last(task_ready_order_{running_task}, running_id), -1)"
        for other_task in range(max_tasks):
            if running_task == other_task:
                continue
            other_ready = f"default(last(task_state_{other_task}, running_id), {STATE_CREATED}) == {STATE_READY}"
            other_priority = f"default(last(task_ready_priority_{other_task}, running_id), -1)"
            other_order = f"default(last(task_ready_order_{other_task}, running_id), -1)"
            priority_terms.append(f"(running_id == {running_task} && {other_ready} && {other_priority} > running_prio)")
            fifo_terms.append(
                f"(running_id == {running_task} && {other_ready} && "
                f"{other_priority} == running_prio && {other_order} >= 0 && "
                f"{other_order} < {chosen_order})"
            )

    return f"""def higher_priority_task_ready :=
  {or_terms(priority_terms)}

def scheduler_fifo_order_bad :=
  {or_terms(fifo_terms)}

def violation_priority :=
  filter(running_id, higher_priority_task_ready)

def violation_round_robin :=
  filter(running_id, scheduler_fifo_order_bad)

{emit_pass_fail_pair(
    'highest_priority_runs',
    'priority',
    'running_id >= 0',
    'higher_priority_task_ready',
)}{emit_pass_fail_pair(
    'round_robin_fifo_respected',
    'round_robin',
    'running_id >= 0',
    'scheduler_fifo_order_bad',
)}"""


def emit_tick_rotation(max_tasks: int) -> str:
    same_priority_ready = []
    for current in range(max_tasks):
        for other in range(max_tasks):
            if current == other:
                continue
            same_priority_ready.append(
                f"(default(last(scheduler_current_running, tick), -1) == {current} && "
                f"default(last(task_state_{other}, tick), {STATE_CREATED}) == {STATE_READY} && "
                f"default(last(task_ready_priority_{other}, tick), -1) == "
                f"default(last(scheduler_current_running_prio, tick), -1))"
            )

    return f"""def scheduler_same_priority_ready_at_tick :=
  {or_terms(same_priority_ready)}

def scheduler_tick_rotation_due :=
  tick > 0 &&
  default(last(scheduler_current_running, tick), -1) >= 0 &&
  scheduler_same_priority_ready_at_tick

def scheduler_tick_due_task: Events[Int] =
  merge(
    if scheduler_tick_rotation_due then default(last(scheduler_current_running, tick), -1)
    else -1,
    -1
  )

def scheduler_tick_due_decision: Events[Int] =
  merge(
    if scheduler_tick_rotation_due
      then default(last(scheduler_running_decisions, tick), 0) + 1
    else -1,
    -1
  )

def scheduler_tick_due_time :=
  filter(time(tick), scheduler_tick_rotation_due)

def scheduler_current_decision :=
  default(last(scheduler_running_decisions, running_id), 0) + 1

def scheduler_tick_reselected_same_task :=
  running_id >= 0 &&
  default(last(scheduler_tick_due_decision, running_id), -1) == scheduler_current_decision &&
  default(last(scheduler_tick_due_task, running_id), -1) == running_id

def scheduler_tick_rotation_missing :=
  tick > 0 &&
  default(last(scheduler_tick_due_time, tick), -1) >
    default(last(time(running_id), tick), -1)

def scheduler_violation_tick_reselected :=
  filter(running_id, scheduler_tick_reselected_same_task)

def scheduler_violation_tick_missing :=
  filter(tick, scheduler_tick_rotation_missing)

def violation_tick_rotation :=
  merge(scheduler_violation_tick_reselected, scheduler_violation_tick_missing)

{emit_pass_fail_pair(
    'one_tick_rotation_respected',
    'tick_rotation',
    'merge(running_id, tick) >= 0',
    'merge(scheduler_tick_reselected_same_task, scheduler_tick_rotation_missing)',
)}"""


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


def generate(max_tasks: int, mode: str = "violations") -> str:
    if max_tasks <= 0:
        raise ValueError("max_tasks must be greater than zero")

    parts = [emit_header(), emit_event_model()]
    for task_id in range(max_tasks):
        parts.append(emit_per_task(task_id))
    parts.extend(
        [
            emit_task_id_bounds(max_tasks),
            emit_state_checks(max_tasks),
            emit_event_consistency(max_tasks),
            emit_blocked_check(max_tasks),
            emit_idle_check(max_tasks),
            emit_priority_and_fifo(max_tasks),
            emit_tick_rotation(max_tasks),
            emit_outputs(mode),
        ]
    )
    return "\n".join(parts)
