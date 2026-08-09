STATE_BLOCKED = 3
TASK_ID_NONE = 255

KIND_CREATE = 1
KIND_LOCK_ENTER = 2
KIND_LOCK_EXIT = 3
KIND_BLOCK = 4
KIND_TIMEOUT = 5
KIND_UNLOCK = 6
KIND_WAKE = 7


CHECKS = [
    ("mutex_valid_initialization", "mutex_invalid_create", "mutex_create_id"),
    ("mutex_owner_consistent", "mutex_owner_discontinuity", "mutex_event_id"),
    ("mutex_owner_only_unlock", "mutex_invalid_unlock", "mutex_unlock_id"),
    ("mutex_non_recursive", "mutex_recursive_lock", "mutex_lock_exit_id"),
    ("mutex_locked_acquire_not_successful", "mutex_locked_acquire_succeeded", "mutex_lock_exit_id"),
    ("mutex_lock_lifecycle_valid", "mutex_invalid_lock", "mutex_event_id"),
    ("mutex_blocking_lock_blocks_task", "mutex_blocking_state", "state_id"),
    ("mutex_timeout_not_early", "mutex_timeout_too_early", "mutex_timeout_id"),
    ("mutex_timeout_returns_failure", "mutex_timeout_result", "mutex_lock_exit_id"),
    ("mutex_unlock_wakes_waiter", "mutex_missing_wake", "mutex_event_id"),
    ("mutex_waiter_priority_respected", "mutex_wake_priority", "mutex_wake_id"),
    ("mutex_equal_priority_fifo_respected", "mutex_wake_fifo", "mutex_wake_id"),
    ("mutex_wait_lifecycle_valid", "mutex_invalid_wait_lifecycle", "mutex_event_id"),
    ("mutex_monitor_bound_respected", "mutex_untracked_mutex", "mutex_event_id"),
]


def nested_merge(streams: list[str], fallback: str) -> str:
    if not streams:
        return fallback

    result = streams[-1]

    for stream in reversed(streams[:-1]):
        result = f"merge({stream}, {result})"

    return result


def or_terms(terms: list[str]) -> str:
    return "false" if not terms else " ||\n  ".join(terms)


def and_terms(terms: list[str]) -> str:
    return "true" if not terms else " &&\n  ".join(terms)


def task_ids(max_tasks: int) -> range:
    return range(max_tasks)


def emit_header() -> str:
    return """in mutex_create_id: Events[Int]

in mutex_lock_enter_id: Events[Int]
in mutex_lock_enter_task: Events[Int]
in mutex_lock_enter_owner: Events[Int]
in mutex_lock_enter_timeout: Events[Int]
in mutex_lock_enter_finite: Events[Int]

in mutex_lock_exit_id: Events[Int]
in mutex_lock_exit_task: Events[Int]
in mutex_lock_exit_owner: Events[Int]
in mutex_lock_exit_succeeded: Events[Int]

in mutex_block_id: Events[Int]
in mutex_block_task: Events[Int]
in mutex_block_prio: Events[Int]
in mutex_block_owner: Events[Int]
in mutex_block_timeout: Events[Int]
in mutex_block_finite: Events[Int]

in mutex_timeout_id: Events[Int]
in mutex_timeout_task: Events[Int]
in mutex_timeout_owner: Events[Int]

in mutex_unlock_id: Events[Int]
in mutex_unlock_task: Events[Int]
in mutex_unlock_owner_before: Events[Int]
in mutex_unlock_owner_after: Events[Int]
in mutex_unlock_succeeded: Events[Int]

in mutex_wake_id: Events[Int]
in mutex_wake_task: Events[Int]
in mutex_wake_prio: Events[Int]

in state_id: Events[Int]
in state_old: Events[Int]
in state_new: Events[Int]

in tick: Events[Int]

"""


def emit_event_model() -> str:
    kinds = [
        ("create", "mutex_create_id", KIND_CREATE),
        ("lock_enter", "mutex_lock_enter_id", KIND_LOCK_ENTER),
        ("lock_exit", "mutex_lock_exit_id", KIND_LOCK_EXIT),
        ("block", "mutex_block_id", KIND_BLOCK),
        ("timeout", "mutex_timeout_id", KIND_TIMEOUT),
        ("unlock", "mutex_unlock_id", KIND_UNLOCK),
        ("wake", "mutex_wake_id", KIND_WAKE),
    ]
    lines: list[str] = []
    kind_streams: list[str] = []
    id_streams: list[str] = []

    for name, id_stream, kind in kinds:
        lines.append(f"def mutex_kind_{name} := if {id_stream} >= 0 then {kind} else 0")
        kind_streams.append(f"mutex_kind_{name}")
        id_streams.append(id_stream)

    lines.extend(
        [
            "",
            f"def mutex_event_kind := {nested_merge(kind_streams, '0')}",
            f"def mutex_event_id := {nested_merge(id_streams, '0')}",
            "",
            "def mutex_tick_sum: Events[Int] =",
            "  merge(if tick > 0 then default(last(mutex_tick_sum, tick), 0) + tick else 0, 0)",
            "",
            "def mutex_block_sequence: Events[Int] =",
            "  merge(default(last(mutex_block_sequence, mutex_block_id), 0) + 1, 0)",
            "",
            "def mutex_state_sequence: Events[Int] =",
            "  merge(default(last(mutex_state_sequence, state_id), 0) + 1, 0)",
            "",
            "def mutex_state_sequence_at_block :=",
            "  default(last(mutex_state_sequence, mutex_block_id), 0)",
            "",
        ]
    )

    return "\n".join(lines)


def emit_task_model(task: int) -> str:
    return f"""# ---------------- Mutex task {task} ----------------
def mutex_enter_for_task_{task} :=
  filter(mutex_lock_enter_id, mutex_lock_enter_task == {task})

def mutex_exit_for_task_{task} :=
  filter(mutex_lock_exit_id, mutex_lock_exit_task == {task})

def mutex_block_for_task_{task} :=
  filter(mutex_block_id, mutex_block_task == {task})

def mutex_timeout_for_task_{task} :=
  filter(mutex_timeout_id, mutex_timeout_task == {task})

def mutex_wake_for_task_{task} :=
  filter(mutex_wake_id, mutex_wake_task == {task})

def mutex_blocked_state_for_task_{task} :=
  filter(state_id, state_id == {task} && state_new == {STATE_BLOCKED})

def mutex_lock_active_task_{task}: Events[Bool] =
  {nested_merge([
      f'if mutex_enter_for_task_{task} >= 0 then true else false',
      f'if mutex_exit_for_task_{task} >= 0 then false else true',
      'false',
  ], 'false')}

def mutex_lock_blocked_task_{task}: Events[Bool] =
  {nested_merge([
      f'if mutex_block_for_task_{task} >= 0 then true else false',
      f'if mutex_enter_for_task_{task} >= 0 then false else true',
      f'if mutex_exit_for_task_{task} >= 0 then false else true',
      'false',
  ], 'false')}

def mutex_lock_resolution_task_{task}: Events[Int] =
  {nested_merge([
      f'if mutex_wake_for_task_{task} >= 0 then 1 else 0',
      f'if mutex_timeout_for_task_{task} >= 0 then 0 else 1',
      f'if mutex_enter_for_task_{task} >= 0 then -1 else 0',
      '-1',
  ], '-1')}

def mutex_enter_object_task_{task}: Events[Int] =
  merge(mutex_enter_for_task_{task}, -1)

def mutex_enter_owner_task_{task}: Events[Int] =
  merge(filter(mutex_lock_enter_owner, mutex_lock_enter_task == {task}), {TASK_ID_NONE})

def mutex_enter_timeout_task_{task}: Events[Int] =
  merge(filter(mutex_lock_enter_timeout, mutex_lock_enter_task == {task}), 0)

def mutex_enter_finite_task_{task}: Events[Int] =
  merge(filter(mutex_lock_enter_finite, mutex_lock_enter_task == {task}), 0)

def mutex_waiting_task_{task}: Events[Bool] =
  {nested_merge([
      f'if mutex_block_for_task_{task} >= 0 then true else false',
      f'if mutex_wake_for_task_{task} >= 0 then false else true',
      f'if mutex_timeout_for_task_{task} >= 0 then false else true',
      'false',
  ], 'false')}

def mutex_wait_object_task_{task}: Events[Int] =
  merge(mutex_block_for_task_{task}, -1)

def mutex_wait_priority_task_{task}: Events[Int] =
  merge(filter(mutex_block_prio, mutex_block_task == {task}), -1)

def mutex_wait_order_task_{task}: Events[Int] =
  merge(filter(mutex_block_sequence, mutex_block_task == {task}), -1)

def mutex_wait_timeout_task_{task}: Events[Int] =
  merge(filter(mutex_block_timeout, mutex_block_task == {task}), 0)

def mutex_wait_finite_task_{task}: Events[Int] =
  merge(filter(mutex_block_finite, mutex_block_task == {task}), 0)

def mutex_wait_start_tick_task_{task}: Events[Int] =
  merge(default(last(mutex_tick_sum, mutex_block_for_task_{task}), 0), -1)

def mutex_block_unconfirmed_task_{task}: Events[Bool] =
  {nested_merge([
      f'if mutex_block_for_task_{task} >= 0 then true else false',
      f'if mutex_blocked_state_for_task_{task} >= 0 then false else true',
      'false',
  ], 'false')}

"""


def emit_slot_assignment(max_mutexes: int) -> str:
    parts = ["# ---------------- Dynamic mutex monitor slots ----------------"]

    for slot in range(max_mutexes):
        own_before = f"default(last(mutex_slot_id_{slot}, mutex_create_id), -1)"
        prior_full = [f"default(last(mutex_slot_id_{i}, mutex_create_id), -1) >= 0" for i in range(slot)]
        new_for_prior = [
            f"default(last(mutex_slot_id_{i}, mutex_create_id), -1) != mutex_create_id"
            for i in range(slot)
        ]
        condition = and_terms([f"{own_before} < 0", *prior_full, *new_for_prior])
        parts.append(
            f"""def mutex_slot_id_{slot}: Events[Int] =
  merge(if {condition} then mutex_create_id else {own_before}, -1)
"""
        )

    known = [
        f"default(last(mutex_slot_id_{slot}, mutex_create_id), -1) == mutex_create_id"
        for slot in range(max_mutexes)
    ]
    empty = [
        f"default(last(mutex_slot_id_{slot}, mutex_create_id), -1) < 0"
        for slot in range(max_mutexes)
    ]
    parts.append(
        f"""def mutex_create_exceeds_bound :=
  ({or_terms(known)}) == false &&
  ({or_terms(empty)}) == false

def mutex_violation_untracked_create :=
  filter(mutex_create_id, mutex_create_exceeds_bound)
"""
    )

    operation_ids = [
        "mutex_lock_enter_id",
        "mutex_lock_exit_id",
        "mutex_block_id",
        "mutex_timeout_id",
        "mutex_unlock_id",
        "mutex_wake_id",
    ]
    violations = ["mutex_violation_untracked_create"]

    for operation_id in operation_ids:
        matches = [
            f"default(last(mutex_slot_id_{slot}, {operation_id}), -1) == {operation_id}"
            for slot in range(max_mutexes)
        ]
        suffix = operation_id.removeprefix("mutex_").removesuffix("_id")
        violation = f"mutex_violation_untracked_{suffix}"
        parts.append(
            f"""def mutex_unknown_{suffix} :=
  ({or_terms(matches)}) == false

def {violation} :=
  filter({operation_id}, mutex_unknown_{suffix})
"""
        )
        violations.append(violation)

    parts.append(
        "def violation_mutex_untracked_mutex :=\n  "
        + nested_merge(violations, "filter(mutex_event_id, false)")
        + "\n"
    )
    return "\n".join(parts)


def slot_match(slot: int, stream: str) -> str:
    return f"default(last(mutex_slot_id_{slot}, {stream}), -1) == {stream}"


def emit_slot_model(slot: int) -> str:
    return f"""# ---------------- Mutex monitor slot {slot} ----------------
def mutex_slot_create_{slot} :=
  filter(mutex_create_id, mutex_create_id == mutex_slot_id_{slot})

def mutex_slot_create_owner_{slot} :=
  if mutex_slot_create_{slot} >= 0 then {TASK_ID_NONE} else -1

def mutex_slot_lock_exit_owner_{slot} :=
  filter(mutex_lock_exit_owner, {slot_match(slot, 'mutex_lock_exit_id')})

def mutex_slot_unlock_owner_{slot} :=
  filter(mutex_unlock_owner_after, {slot_match(slot, 'mutex_unlock_id')})

def mutex_slot_owner_{slot}: Events[Int] =
  {nested_merge([
      f'mutex_slot_create_owner_{slot}',
      f'mutex_slot_lock_exit_owner_{slot}',
      f'mutex_slot_unlock_owner_{slot}',
      '-1',
  ], '-1')}

"""


def waiting_terms(max_tasks: int, trigger: str) -> list[str]:
    return [
        f"(default(last(mutex_waiting_task_{task}, {trigger}), false) && "
        f"default(last(mutex_wait_object_task_{task}, {trigger}), -1) == {trigger})"
        for task in task_ids(max_tasks)
    ]


def emit_create_checks(max_tasks: int, max_mutexes: int) -> str:
    owned_terms = [
        f"({slot_match(slot, 'mutex_create_id')} && "
        f"default(last(mutex_slot_owner_{slot}, mutex_create_id), {TASK_ID_NONE}) != {TASK_ID_NONE})"
        for slot in range(max_mutexes)
    ]
    return f"""# ---------------- Initialization checks ----------------
def mutex_create_has_waiter :=
  {or_terms(waiting_terms(max_tasks, 'mutex_create_id'))}

def mutex_create_is_owned :=
  {or_terms(owned_terms)}

def mutex_invalid_create_bad :=
  mutex_create_has_waiter || mutex_create_is_owned

def violation_mutex_invalid_create :=
  filter(mutex_create_id, mutex_invalid_create_bad)

"""


def emit_owner_checks(max_tasks: int, max_mutexes: int) -> str:
    valid_owner = " || ".join(
        [f"OWNER == {task}" for task in task_ids(max_tasks)] + [f"OWNER == {TASK_ID_NONE}"]
    )
    continuity_streams: list[str] = []
    parts = ["# ---------------- Ownership checks ----------------"]

    for slot in range(max_mutexes):
        enter_match = slot_match(slot, "mutex_lock_enter_id")
        block_match = slot_match(slot, "mutex_block_id")
        timeout_match = slot_match(slot, "mutex_timeout_id")
        unlock_match = slot_match(slot, "mutex_unlock_id")
        exit_match = slot_match(slot, "mutex_lock_exit_id")
        owner_before = f"default(last(mutex_slot_owner_{slot}, mutex_lock_exit_id), {TASK_ID_NONE})"
        exit_blocked = [
            f"(mutex_lock_exit_task == {task} && "
            f"default(last(mutex_lock_blocked_task_{task}, mutex_lock_exit_id), false))"
            for task in task_ids(max_tasks)
        ]
        exit_expected = (
            f"(if mutex_lock_exit_succeeded == 1 && ({or_terms(exit_blocked)}) == false "
            f"then mutex_lock_exit_task else {owner_before})"
        )
        checks = {
            "enter": (
                "mutex_lock_enter_id",
                f"{enter_match} && mutex_lock_enter_owner != "
                f"default(last(mutex_slot_owner_{slot}, mutex_lock_enter_id), {TASK_ID_NONE})",
            ),
            "block": (
                "mutex_block_id",
                f"{block_match} && mutex_block_owner != "
                f"default(last(mutex_slot_owner_{slot}, mutex_block_id), {TASK_ID_NONE})",
            ),
            "timeout": (
                "mutex_timeout_id",
                f"{timeout_match} && mutex_timeout_owner != "
                f"default(last(mutex_slot_owner_{slot}, mutex_timeout_id), {TASK_ID_NONE})",
            ),
            "unlock": (
                "mutex_unlock_id",
                f"{unlock_match} && mutex_unlock_owner_before != "
                f"default(last(mutex_slot_owner_{slot}, mutex_unlock_id), {TASK_ID_NONE})",
            ),
            "exit": (
                "mutex_lock_exit_id",
                f"{exit_match} && mutex_lock_exit_owner != {exit_expected}",
            ),
        }

        for suffix, (stream, condition) in checks.items():
            name = f"mutex_violation_owner_{suffix}_{slot}"
            parts.append(
                f"""def mutex_owner_bad_{suffix}_{slot} :=
  {condition}

def {name} :=
  filter({stream}, mutex_owner_bad_{suffix}_{slot})
"""
            )
            continuity_streams.append(name)

    owner_fields = [
        ("enter", "mutex_lock_enter_id", "mutex_lock_enter_owner"),
        ("exit", "mutex_lock_exit_id", "mutex_lock_exit_owner"),
        ("block", "mutex_block_id", "mutex_block_owner"),
        ("timeout", "mutex_timeout_id", "mutex_timeout_owner"),
        ("unlock_before", "mutex_unlock_id", "mutex_unlock_owner_before"),
        ("unlock_after", "mutex_unlock_id", "mutex_unlock_owner_after"),
    ]

    for suffix, stream, field in owner_fields:
        expression = valid_owner.replace("OWNER", field)
        name = f"mutex_violation_owner_id_{suffix}"
        parts.append(
            f"""def mutex_owner_id_bad_{suffix} :=
  ({expression}) == false

def {name} :=
  filter({stream}, mutex_owner_id_bad_{suffix})
"""
        )
        continuity_streams.append(name)

    parts.append(
        "def violation_mutex_owner_discontinuity :=\n  "
        + nested_merge(continuity_streams, "filter(mutex_event_id, false)")
        + "\n"
    )
    return "\n".join(parts)


def emit_lock_checks(max_tasks: int) -> str:
    valid_task = " || ".join(f"mutex_lock_enter_task == {task}" for task in task_ids(max_tasks))
    valid_exit_task = " || ".join(f"mutex_lock_exit_task == {task}" for task in task_ids(max_tasks))
    valid_block_task = " || ".join(f"mutex_block_task == {task}" for task in task_ids(max_tasks))
    enter_bad = [
        f"(({valid_task}) == false)",
        "mutex_lock_enter_finite < 0 || mutex_lock_enter_finite > 1",
    ]
    exit_bad = [
        f"(({valid_exit_task}) == false)",
        "mutex_lock_exit_succeeded < 0 || mutex_lock_exit_succeeded > 1",
    ]
    block_bad: list[str] = []
    recursive: list[str] = []
    locked_success: list[str] = []
    timeout_result: list[str] = []

    for task in task_ids(max_tasks):
        active_enter = f"default(last(mutex_lock_active_task_{task}, mutex_lock_enter_id), false)"
        active_exit = f"default(last(mutex_lock_active_task_{task}, mutex_lock_exit_id), false)"
        blocked_exit = f"default(last(mutex_lock_blocked_task_{task}, mutex_lock_exit_id), false)"
        resolution = f"default(last(mutex_lock_resolution_task_{task}, mutex_lock_exit_id), -1)"
        enter_owner = f"default(last(mutex_enter_owner_task_{task}, mutex_lock_exit_id), {TASK_ID_NONE})"

        enter_bad.append(f"(mutex_lock_enter_task == {task} && {active_enter})")
        exit_bad.append(
            f"(mutex_lock_exit_task == {task} && ("
            f"{active_exit} == false || "
            f"default(last(mutex_enter_object_task_{task}, mutex_lock_exit_id), -1) != mutex_lock_exit_id || "
            f"({blocked_exit} && {resolution} != mutex_lock_exit_succeeded)))"
        )
        block_bad.append(
            f"(mutex_block_task == {task} && ("
            f"default(last(mutex_lock_active_task_{task}, mutex_block_id), false) == false || "
            f"default(last(mutex_lock_blocked_task_{task}, mutex_block_id), false) || "
            f"default(last(mutex_enter_object_task_{task}, mutex_block_id), -1) != mutex_block_id || "
            f"default(last(mutex_enter_owner_task_{task}, mutex_block_id), {TASK_ID_NONE}) != mutex_block_owner || "
            f"default(last(mutex_enter_timeout_task_{task}, mutex_block_id), -1) != mutex_block_timeout || "
            f"default(last(mutex_enter_finite_task_{task}, mutex_block_id), -1) != mutex_block_finite))"
        )
        recursive.append(
            f"(mutex_lock_exit_task == {task} && mutex_lock_exit_succeeded == 1 && "
            f"{blocked_exit} == false && {enter_owner} == {task})"
        )
        locked_success.append(
            f"(mutex_lock_exit_task == {task} && mutex_lock_exit_succeeded == 1 && "
            f"{blocked_exit} == false && {enter_owner} != {TASK_ID_NONE} && {enter_owner} != {task})"
        )
        timeout_result.append(
            f"(mutex_lock_exit_task == {task} && {blocked_exit} && {resolution} == 0 && "
            f"mutex_lock_exit_succeeded != 0)"
        )

    return f"""# ---------------- Lock checks ----------------
def mutex_invalid_lock_enter_bad :=
  {or_terms(enter_bad)}

def mutex_invalid_lock_exit_bad :=
  {or_terms(exit_bad)}

def mutex_invalid_lock_block_bad :=
  (({valid_block_task}) == false) ||
  mutex_block_owner == {TASK_ID_NONE} ||
  mutex_block_finite < 0 || mutex_block_finite > 1 ||
  (mutex_block_finite == 1 && mutex_block_timeout <= 0) ||
  {or_terms(block_bad)}

def mutex_violation_invalid_lock_enter :=
  filter(mutex_lock_enter_id, mutex_invalid_lock_enter_bad)

def mutex_violation_invalid_lock_exit :=
  filter(mutex_lock_exit_id, mutex_invalid_lock_exit_bad)

def mutex_violation_invalid_lock_block :=
  filter(mutex_block_id, mutex_invalid_lock_block_bad)

def violation_mutex_invalid_lock :=
  {nested_merge([
      'mutex_violation_invalid_lock_enter',
      'mutex_violation_invalid_lock_exit',
      'mutex_violation_invalid_lock_block',
  ], 'filter(mutex_event_id, false)')}

def mutex_recursive_lock_bad :=
  {or_terms(recursive)}

def violation_mutex_recursive_lock :=
  filter(mutex_lock_exit_id, mutex_recursive_lock_bad)

def mutex_locked_acquire_succeeded_bad :=
  {or_terms(locked_success)}

def violation_mutex_locked_acquire_succeeded :=
  filter(mutex_lock_exit_id, mutex_locked_acquire_succeeded_bad)

def mutex_timeout_result_bad :=
  {or_terms(timeout_result)}

def violation_mutex_timeout_result :=
  filter(mutex_lock_exit_id, mutex_timeout_result_bad)

"""


def emit_blocking_state_checks(max_tasks: int) -> str:
    task_match = [
        f"(state_id == {task} && default(last(mutex_block_task, state_id), -1) == {task})"
        for task in task_ids(max_tasks)
    ]
    unresolved_wake = [
        f"(mutex_wake_task == {task} && "
        f"default(last(mutex_block_unconfirmed_task_{task}, mutex_wake_id), false))"
        for task in task_ids(max_tasks)
    ]
    unresolved_timeout = [
        f"(mutex_timeout_task == {task} && "
        f"default(last(mutex_block_unconfirmed_task_{task}, mutex_timeout_id), false))"
        for task in task_ids(max_tasks)
    ]
    return f"""# ---------------- Blocking-state checks ----------------
def mutex_first_state_after_block :=
  default(last(mutex_state_sequence_at_block, state_id), -1) ==
  default(last(mutex_state_sequence, state_id), 0)

def mutex_blocking_state_bad :=
  mutex_first_state_after_block &&
  (({or_terms(task_match)}) == false || state_new != {STATE_BLOCKED})

def mutex_violation_blocking_state_transition :=
  filter(state_id, mutex_blocking_state_bad)

def mutex_violation_wake_without_blocked_state :=
  filter(mutex_wake_id, {or_terms(unresolved_wake)})

def mutex_violation_timeout_without_blocked_state :=
  filter(mutex_timeout_id, {or_terms(unresolved_timeout)})

def violation_mutex_blocking_state :=
  merge(mutex_violation_blocking_state_transition,
        merge(mutex_violation_wake_without_blocked_state,
              mutex_violation_timeout_without_blocked_state))

"""


def emit_timeout_checks(max_tasks: int) -> str:
    valid_task = [f"mutex_timeout_task == {task}" for task in task_ids(max_tasks)]
    lifecycle: list[str] = []
    early: list[str] = []

    for task in task_ids(max_tasks):
        lifecycle.append(
            f"(mutex_timeout_task == {task} && ("
            f"default(last(mutex_waiting_task_{task}, mutex_timeout_id), false) == false || "
            f"default(last(mutex_wait_object_task_{task}, mutex_timeout_id), -1) != mutex_timeout_id || "
            f"default(last(mutex_wait_finite_task_{task}, mutex_timeout_id), 0) != 1))"
        )
        early.append(
            f"(mutex_timeout_task == {task} && "
            f"default(last(mutex_tick_sum, mutex_timeout_id), 0) - "
            f"default(last(mutex_wait_start_tick_task_{task}, mutex_timeout_id), 0) < "
            f"default(last(mutex_wait_timeout_task_{task}, mutex_timeout_id), 0))"
        )

    return f"""# ---------------- Timeout checks ----------------
def mutex_timeout_lifecycle_bad :=
  (({or_terms(valid_task)}) == false) ||
  {or_terms(lifecycle)}

def mutex_violation_timeout_lifecycle :=
  filter(mutex_timeout_id, mutex_timeout_lifecycle_bad)

def mutex_timeout_too_early_bad :=
  {or_terms(early)}

def violation_mutex_timeout_too_early :=
  filter(mutex_timeout_id, mutex_timeout_too_early_bad)

"""


def emit_unlock_and_wake_checks(max_tasks: int) -> str:
    has_waiter = or_terms(waiting_terms(max_tasks, "mutex_unlock_id"))
    valid_unlock_task = " || ".join(f"mutex_unlock_task == {task}" for task in task_ids(max_tasks))
    valid_wake_task = " || ".join(f"mutex_wake_task == {task}" for task in task_ids(max_tasks))
    handoff_targets = [
        f"(mutex_unlock_owner_after == {task} && "
        f"default(last(mutex_waiting_task_{task}, mutex_unlock_id), false) && "
        f"default(last(mutex_wait_object_task_{task}, mutex_unlock_id), -1) == mutex_unlock_id)"
        for task in task_ids(max_tasks)
    ]
    wake_lifecycle: list[str] = []
    priority: list[str] = []
    fifo: list[str] = []

    for chosen in task_ids(max_tasks):
        chosen_waiting = f"default(last(mutex_waiting_task_{chosen}, mutex_wake_id), false)"
        chosen_object = f"default(last(mutex_wait_object_task_{chosen}, mutex_wake_id), -1)"
        chosen_priority = f"default(last(mutex_wait_priority_task_{chosen}, mutex_wake_id), -1)"
        chosen_order = f"default(last(mutex_wait_order_task_{chosen}, mutex_wake_id), -1)"
        wake_lifecycle.append(
            f"(mutex_wake_task == {chosen} && ("
            f"{chosen_waiting} == false || {chosen_object} != mutex_wake_id || "
            f"{chosen_priority} != mutex_wake_prio))"
        )

        for other in task_ids(max_tasks):
            if chosen == other:
                continue

            other_waiting = f"default(last(mutex_waiting_task_{other}, mutex_wake_id), false)"
            other_object = f"default(last(mutex_wait_object_task_{other}, mutex_wake_id), -1)"
            other_priority = f"default(last(mutex_wait_priority_task_{other}, mutex_wake_id), -1)"
            other_order = f"default(last(mutex_wait_order_task_{other}, mutex_wake_id), -1)"
            priority.append(
                f"(mutex_wake_task == {chosen} && {other_waiting} && "
                f"{other_object} == mutex_wake_id && {other_priority} > {chosen_priority})"
            )
            fifo.append(
                f"(mutex_wake_task == {chosen} && {other_waiting} && "
                f"{other_object} == mutex_wake_id && {other_priority} == {chosen_priority} && "
                f"{other_order} < {chosen_order})"
            )

    return f"""# ---------------- Unlock and wake checks ----------------
def mutex_unlock_has_waiter :=
  {has_waiter}

def mutex_unlock_requires_wake :=
  mutex_unlock_succeeded == 1 && mutex_unlock_has_waiter

def mutex_invalid_unlock_bad :=
  (({valid_unlock_task}) == false) ||
  mutex_unlock_succeeded < 0 || mutex_unlock_succeeded > 1 ||
  (mutex_unlock_task != mutex_unlock_owner_before &&
   (mutex_unlock_succeeded != 0 || mutex_unlock_owner_after != mutex_unlock_owner_before)) ||
  (mutex_unlock_task == mutex_unlock_owner_before &&
   mutex_unlock_succeeded != 1) ||
  (mutex_unlock_succeeded == 1 && mutex_unlock_has_waiter == false &&
   mutex_unlock_owner_after != {TASK_ID_NONE}) ||
  (mutex_unlock_succeeded == 1 && mutex_unlock_has_waiter &&
   ({or_terms(handoff_targets)}) == false)

def violation_mutex_invalid_unlock :=
  filter(mutex_unlock_id, mutex_invalid_unlock_bad)

def mutex_previous_event_required_wake :=
  default(last(mutex_event_kind, mutex_event_kind), 0) == {KIND_UNLOCK} &&
  default(last(mutex_unlock_requires_wake, mutex_event_kind), false)

def mutex_missing_wake_bad :=
  mutex_previous_event_required_wake && mutex_event_kind != {KIND_WAKE}

def violation_mutex_missing_wake :=
  filter(mutex_event_id, mutex_missing_wake_bad)

def mutex_wake_not_after_unlock :=
  default(last(mutex_event_kind, mutex_wake_id), 0) != {KIND_UNLOCK} ||
  default(last(mutex_unlock_requires_wake, mutex_wake_id), false) == false ||
  default(last(mutex_unlock_id, mutex_wake_id), -1) != mutex_wake_id ||
  default(last(mutex_unlock_owner_after, mutex_wake_id), {TASK_ID_NONE}) != mutex_wake_task

def mutex_wake_lifecycle_bad :=
  (({valid_wake_task}) == false) ||
  mutex_wake_not_after_unlock ||
  {or_terms(wake_lifecycle)}

def mutex_violation_wake_lifecycle :=
  filter(mutex_wake_id, mutex_wake_lifecycle_bad)

def mutex_wake_priority_bad :=
  {or_terms(priority)}

def violation_mutex_wake_priority :=
  filter(mutex_wake_id, mutex_wake_priority_bad)

def mutex_wake_fifo_bad :=
  {or_terms(fifo)}

def violation_mutex_wake_fifo :=
  filter(mutex_wake_id, mutex_wake_fifo_bad)

def violation_mutex_invalid_wait_lifecycle :=
  merge(mutex_violation_timeout_lifecycle, mutex_violation_wake_lifecycle)

"""


def emit_outputs(mode: str) -> str:
    lines: list[str] = []

    if mode == "violations":
        for _, internal, _ in CHECKS:
            lines.append(f"out violation_{internal}")
    elif mode == "checks":
        for public, internal, trigger in CHECKS:
            lines.append(f"def FAIL_{public} := violation_{internal}")
            lines.append(
                f"def mutex_last_failure_time_{internal} := "
                f"merge(time(violation_{internal}), "
                f"default(last(time(violation_{internal}), {trigger}), -1))"
            )
            lines.append(
                f"def PASS_{public} := filter({trigger}, "
                f"time({trigger}) != mutex_last_failure_time_{internal})"
            )
            lines.append(f"out FAIL_{public}")
            lines.append(f"out PASS_{public}")
    else:
        raise ValueError(f"invalid mode: {mode}")

    return "\n".join(lines) + "\n"


def generate(max_tasks: int, max_mutexes: int, mode: str = "violations") -> str:
    if max_tasks <= 0 or max_tasks >= TASK_ID_NONE:
        raise ValueError("max_tasks must be in the range 1..254")
    if max_mutexes <= 0:
        raise ValueError("max_mutexes must be greater than 0")

    parts = [emit_header(), emit_event_model()]

    for task in task_ids(max_tasks):
        parts.append(emit_task_model(task))

    parts.append(emit_slot_assignment(max_mutexes))

    for slot in range(max_mutexes):
        parts.append(emit_slot_model(slot))

    parts.extend(
        [
            emit_create_checks(max_tasks, max_mutexes),
            emit_owner_checks(max_tasks, max_mutexes),
            emit_lock_checks(max_tasks),
            emit_blocking_state_checks(max_tasks),
            emit_timeout_checks(max_tasks),
            emit_unlock_and_wake_checks(max_tasks),
            emit_outputs(mode),
        ]
    )
    return "\n".join(parts)
