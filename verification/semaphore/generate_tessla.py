#!/usr/bin/env python3

STATE_BLOCKED = 3
TASK_ID_NONE = 255

KIND_CREATE = 1
KIND_ACQUIRE_ENTER = 2
KIND_ACQUIRE_EXIT = 3
KIND_BLOCK = 4
KIND_TIMEOUT = 5
KIND_RELEASE = 6
KIND_WAKE = 7


CHECKS = [
    ("sem_valid_initialization", "sem_invalid_create", "sem_create_id"),
    ("sem_count_within_capacity", "sem_count_out_of_range", "sem_event_id"),
    ("sem_count_transitions_consistent", "sem_count_discontinuity", "sem_event_id"),
    ("sem_full_release_rejected", "sem_invalid_release", "sem_release_id"),
    ("sem_empty_acquire_not_successful", "sem_empty_acquire_succeeded", "sem_acquire_exit_id"),
    ("sem_acquire_lifecycle_valid", "sem_invalid_acquire", "sem_event_id"),
    ("sem_blocking_acquire_blocks_task", "sem_blocking_state", "state_id"),
    ("sem_timeout_not_early", "sem_timeout_too_early", "sem_timeout_id"),
    ("sem_timeout_returns_failure", "sem_timeout_result", "sem_acquire_exit_id"),
    ("sem_release_wakes_waiter", "sem_missing_wake", "sem_event_id"),
    ("sem_waiter_priority_respected", "sem_wake_priority", "sem_wake_id"),
    ("sem_equal_priority_fifo_respected", "sem_wake_fifo", "sem_wake_id"),
    ("sem_wait_lifecycle_valid", "sem_invalid_wait_lifecycle", "sem_event_id"),
    ("sem_monitor_bound_respected", "sem_untracked_semaphore", "sem_event_id"),
]


def emit_header() -> str:
    return """in sem_create_id: Events[Int]
in sem_create_initial_count: Events[Int]
in sem_create_max_count: Events[Int]

in sem_acquire_enter_id: Events[Int]
in sem_acquire_enter_task: Events[Int]
in sem_acquire_enter_count: Events[Int]
in sem_acquire_enter_timeout: Events[Int]
in sem_acquire_enter_finite: Events[Int]

in sem_acquire_exit_id: Events[Int]
in sem_acquire_exit_task: Events[Int]
in sem_acquire_exit_count: Events[Int]
in sem_acquire_exit_succeeded: Events[Int]

in sem_block_id: Events[Int]
in sem_block_task: Events[Int]
in sem_block_prio: Events[Int]
in sem_block_timeout: Events[Int]
in sem_block_finite: Events[Int]

in sem_timeout_id: Events[Int]
in sem_timeout_task: Events[Int]
in sem_timeout_count: Events[Int]

in sem_release_id: Events[Int]
in sem_release_count_before: Events[Int]
in sem_release_count_after: Events[Int]
in sem_release_max_count: Events[Int]
in sem_release_succeeded: Events[Int]

in sem_wake_id: Events[Int]
in sem_wake_task: Events[Int]
in sem_wake_prio: Events[Int]

in state_id: Events[Int]
in state_old: Events[Int]
in state_new: Events[Int]

in tick: Events[Int]

"""


def nested_merge(streams: list[str], fallback: str) -> str:
    if not streams:
        return fallback

    result = streams[-1]

    for stream in reversed(streams[:-1]):
        result = f"merge({stream}, {result})"

    return result


def or_terms(terms: list[str]) -> str:
    if not terms:
        return "false"

    return " ||\n  ".join(terms)


def and_terms(terms: list[str]) -> str:
    if not terms:
        return "true"

    return " &&\n  ".join(terms)


def configured_task_ids(max_tasks: int) -> range:
    """Application IDs are 0..max_tasks-1; idle uses ID max_tasks."""
    return range(max_tasks + 1)


def emit_event_model() -> str:
    kinds = [
        ("create", "sem_create_id", KIND_CREATE),
        ("acquire_enter", "sem_acquire_enter_id", KIND_ACQUIRE_ENTER),
        ("acquire_exit", "sem_acquire_exit_id", KIND_ACQUIRE_EXIT),
        ("block", "sem_block_id", KIND_BLOCK),
        ("timeout", "sem_timeout_id", KIND_TIMEOUT),
        ("release", "sem_release_id", KIND_RELEASE),
        ("wake", "sem_wake_id", KIND_WAKE),
    ]

    lines = []
    kind_streams = []
    id_streams = []

    for name, id_stream, kind in kinds:
        lines.append(f"def sem_kind_{name} := if {id_stream} >= 0 then {kind} else 0")
        kind_streams.append(f"sem_kind_{name}")
        id_streams.append(id_stream)

    lines.append("")
    lines.append(f"def sem_event_kind := {nested_merge(kind_streams, '0')}")
    lines.append(f"def sem_event_id := {nested_merge(id_streams, '0')}")
    lines.append("")
    lines.append("def sem_tick_sum: Events[Int] =")
    lines.append("  merge(")
    lines.append("    if tick > 0 then default(last(sem_tick_sum, tick), 0) + tick else 0,")
    lines.append("    0")
    lines.append("  )")
    lines.append("")
    lines.append("def sem_block_sequence: Events[Int] =")
    lines.append("  merge(default(last(sem_block_sequence, sem_block_id), 0) + 1, 0)")
    lines.append("")
    lines.append("def sem_state_sequence: Events[Int] =")
    lines.append("  merge(default(last(sem_state_sequence, state_id), 0) + 1, 0)")
    lines.append("")
    lines.append("def sem_state_sequence_at_block :=")
    lines.append("  default(last(sem_state_sequence, sem_block_id), 0)")
    lines.append("")

    return "\n".join(lines)


def emit_task_model(task_id: int) -> str:
    return f"""# ---------------- Semaphore waiter task {task_id} ----------------
def sem_enter_for_task_{task_id} :=
  filter(sem_acquire_enter_id, sem_acquire_enter_task == {task_id})

def sem_exit_for_task_{task_id} :=
  filter(sem_acquire_exit_id, sem_acquire_exit_task == {task_id})

def sem_block_for_task_{task_id} :=
  filter(sem_block_id, sem_block_task == {task_id})

def sem_timeout_for_task_{task_id} :=
  filter(sem_timeout_id, sem_timeout_task == {task_id})

def sem_wake_for_task_{task_id} :=
  filter(sem_wake_id, sem_wake_task == {task_id})

def sem_blocked_state_for_task_{task_id} :=
  filter(state_id, state_id == {task_id} && state_new == {STATE_BLOCKED})

def sem_acquire_active_task_{task_id}: Events[Bool] =
  {nested_merge([
      f'if sem_enter_for_task_{task_id} >= 0 then true else false',
      f'if sem_exit_for_task_{task_id} >= 0 then false else true',
      'false',
  ], 'false')}

def sem_acquire_blocked_task_{task_id}: Events[Bool] =
  {nested_merge([
      f'if sem_block_for_task_{task_id} >= 0 then true else false',
      f'if sem_enter_for_task_{task_id} >= 0 then false else true',
      f'if sem_exit_for_task_{task_id} >= 0 then false else true',
      'false',
  ], 'false')}

def sem_acquire_resolution_task_{task_id}: Events[Int] =
  {nested_merge([
      f'if sem_wake_for_task_{task_id} >= 0 then 1 else 0',
      f'if sem_timeout_for_task_{task_id} >= 0 then 0 else 1',
      f'if sem_enter_for_task_{task_id} >= 0 then -1 else 0',
      '-1',
  ], '-1')}

def sem_enter_object_task_{task_id}: Events[Int] =
  merge(sem_enter_for_task_{task_id}, -1)

def sem_enter_count_task_{task_id}: Events[Int] =
  merge(filter(sem_acquire_enter_count, sem_acquire_enter_task == {task_id}), -1)

def sem_enter_timeout_task_{task_id}: Events[Int] =
  merge(filter(sem_acquire_enter_timeout, sem_acquire_enter_task == {task_id}), 0)

def sem_enter_finite_task_{task_id}: Events[Int] =
  merge(filter(sem_acquire_enter_finite, sem_acquire_enter_task == {task_id}), 0)

def sem_waiting_task_{task_id}: Events[Bool] =
  {nested_merge([
      f'if sem_block_for_task_{task_id} >= 0 then true else false',
      f'if sem_wake_for_task_{task_id} >= 0 then false else true',
      f'if sem_timeout_for_task_{task_id} >= 0 then false else true',
      'false',
  ], 'false')}

def sem_wait_object_task_{task_id}: Events[Int] =
  merge(sem_block_for_task_{task_id}, -1)

def sem_wait_priority_task_{task_id}: Events[Int] =
  merge(filter(sem_block_prio, sem_block_task == {task_id}), -1)

def sem_wait_order_task_{task_id}: Events[Int] =
  merge(filter(sem_block_sequence, sem_block_task == {task_id}), -1)

def sem_wait_timeout_task_{task_id}: Events[Int] =
  merge(filter(sem_block_timeout, sem_block_task == {task_id}), 0)

def sem_wait_finite_task_{task_id}: Events[Int] =
  merge(filter(sem_block_finite, sem_block_task == {task_id}), 0)

def sem_wait_start_tick_task_{task_id}: Events[Int] =
  merge(default(last(sem_tick_sum, sem_block_for_task_{task_id}), 0), -1)

def sem_block_unconfirmed_task_{task_id}: Events[Bool] =
  {nested_merge([
      f'if sem_block_for_task_{task_id} >= 0 then true else false',
      f'if sem_blocked_state_for_task_{task_id} >= 0 then false else true',
      'false',
  ], 'false')}

"""


def emit_taskless_actor_model() -> str:
    task_id = TASK_ID_NONE

    return f"""# ---------------- Taskless acquire actor ({task_id}) ----------------
def sem_enter_for_task_{task_id} :=
  filter(sem_acquire_enter_id, sem_acquire_enter_task == {task_id})

def sem_exit_for_task_{task_id} :=
  filter(sem_acquire_exit_id, sem_acquire_exit_task == {task_id})

def sem_acquire_active_task_{task_id}: Events[Bool] =
  {nested_merge([
      f'if sem_enter_for_task_{task_id} >= 0 then true else false',
      f'if sem_exit_for_task_{task_id} >= 0 then false else true',
      'false',
  ], 'false')}

def sem_acquire_blocked_task_{task_id}: Events[Bool] = false

def sem_acquire_resolution_task_{task_id}: Events[Int] = -1

def sem_enter_object_task_{task_id}: Events[Int] =
  merge(sem_enter_for_task_{task_id}, -1)

def sem_enter_count_task_{task_id}: Events[Int] =
  merge(filter(sem_acquire_enter_count, sem_acquire_enter_task == {task_id}), -1)

def sem_enter_timeout_task_{task_id}: Events[Int] =
  merge(filter(sem_acquire_enter_timeout, sem_acquire_enter_task == {task_id}), 0)

def sem_enter_finite_task_{task_id}: Events[Int] =
  merge(filter(sem_acquire_enter_finite, sem_acquire_enter_task == {task_id}), 0)

"""


def emit_slot_assignment(max_semaphores: int) -> str:
    parts = ["# ---------------- Dynamic semaphore monitor slots ----------------"]

    for slot in range(max_semaphores):
        own_before = f"default(last(sem_slot_id_{slot}, sem_create_id), -1)"
        prior_full = [f"default(last(sem_slot_id_{i}, sem_create_id), -1) >= 0" for i in range(slot)]
        new_for_prior = [f"default(last(sem_slot_id_{i}, sem_create_id), -1) != sem_create_id" for i in range(slot)]
        assign_condition = and_terms([f"{own_before} < 0", *prior_full, *new_for_prior])

        parts.append(f"""def sem_slot_id_{slot}: Events[Int] =
  merge(
    if {assign_condition} then sem_create_id
    else {own_before},
    -1
  )
""")

    known_before = [
        f"default(last(sem_slot_id_{slot}, sem_create_id), -1) == sem_create_id" for slot in range(max_semaphores)
    ]
    empty_before = [f"default(last(sem_slot_id_{slot}, sem_create_id), -1) < 0" for slot in range(max_semaphores)]

    parts.append(f"""def sem_create_exceeds_bound :=
  ({or_terms(known_before)}) == false &&
  ({or_terms(empty_before)}) == false

def sem_violation_untracked_create :=
  filter(sem_create_id, sem_create_exceeds_bound)
""")

    operation_ids = [
        "sem_acquire_enter_id",
        "sem_acquire_exit_id",
        "sem_block_id",
        "sem_timeout_id",
        "sem_release_id",
        "sem_wake_id",
    ]
    unknown_streams = ["sem_violation_untracked_create"]

    for operation_id in operation_ids:
        matches = [
            f"default(last(sem_slot_id_{slot}, {operation_id}), -1) == {operation_id}" for slot in range(max_semaphores)
        ]
        suffix = operation_id.removeprefix("sem_").removesuffix("_id")
        stream_name = f"sem_violation_untracked_{suffix}"
        parts.append(f"""def sem_unknown_{suffix} :=
  ({or_terms(matches)}) == false

def {stream_name} :=
  filter({operation_id}, sem_unknown_{suffix})
""")
        unknown_streams.append(stream_name)

    parts.append(
        f"def violation_sem_untracked_semaphore :=\n  "
        f"{nested_merge(unknown_streams, 'filter(sem_event_id, false)')}\n"
    )

    return "\n".join(parts)


def emit_slot_model(slot: int) -> str:
    match = lambda stream: f"default(last(sem_slot_id_{slot}, {stream}), -1) == {stream}"

    return f"""# ---------------- Semaphore monitor slot {slot} ----------------
def sem_slot_create_initial_{slot} :=
  filter(sem_create_initial_count, sem_create_id == sem_slot_id_{slot})

def sem_slot_create_max_{slot} :=
  filter(sem_create_max_count, sem_create_id == sem_slot_id_{slot})

def sem_slot_enter_count_{slot} :=
  filter(sem_acquire_enter_count, {match('sem_acquire_enter_id')})

def sem_slot_exit_count_{slot} :=
  filter(sem_acquire_exit_count, {match('sem_acquire_exit_id')})

def sem_slot_timeout_count_{slot} :=
  filter(sem_timeout_count, {match('sem_timeout_id')})

def sem_slot_release_before_{slot} :=
  filter(sem_release_count_before, {match('sem_release_id')})

def sem_slot_release_after_{slot} :=
  filter(sem_release_count_after, {match('sem_release_id')})

def sem_slot_release_max_{slot} :=
  filter(sem_release_max_count, {match('sem_release_id')})

def sem_slot_max_{slot}: Events[Int] =
  merge(sem_slot_create_max_{slot}, -1)

def sem_slot_count_{slot}: Events[Int] =
  {nested_merge([
      f'sem_slot_create_initial_{slot}',
      f'sem_slot_release_after_{slot}',
      f'sem_slot_exit_count_{slot}',
      '-1',
  ], '-1')}

"""


def waiting_on_release_terms(max_tasks: int) -> list[str]:
    return [
        f"(default(last(sem_waiting_task_{task}, sem_release_id), false) && "
        f"default(last(sem_wait_object_task_{task}, sem_release_id), -1) == sem_release_id)"
        for task in configured_task_ids(max_tasks)
    ]


def waiting_on_create_terms(max_tasks: int) -> list[str]:
    return [
        f"(default(last(sem_waiting_task_{task}, sem_create_id), false) && "
        f"default(last(sem_wait_object_task_{task}, sem_create_id), -1) == sem_create_id)"
        for task in configured_task_ids(max_tasks)
    ]


def emit_create_checks(max_tasks: int) -> str:
    return f"""# ---------------- Creation checks ----------------
def sem_create_has_waiter :=
  {or_terms(waiting_on_create_terms(max_tasks))}

def sem_invalid_create_bad :=
  sem_create_max_count <= 0 ||
  sem_create_initial_count < 0 ||
  sem_create_initial_count > sem_create_max_count ||
  sem_create_has_waiter

def violation_sem_invalid_create :=
  filter(sem_create_id, sem_invalid_create_bad)

"""


def emit_count_checks(max_semaphores: int, actor_ids: list[int]) -> str:
    range_terms = {
        "create": ["sem_create_initial_count < 0 || sem_create_initial_count > sem_create_max_count"],
        "enter": [],
        "exit": [],
        "timeout": [],
        "release": [],
    }
    discontinuity_streams = []

    blocked_terms = [
        f"(sem_acquire_exit_task == {actor} && "
        f"default(last(sem_acquire_blocked_task_{actor}, sem_acquire_exit_id), false))"
        for actor in actor_ids
    ]

    parts = [
        "# ---------------- Count range and continuity checks ----------------",
        f"def sem_exit_was_blocked :=\n  {or_terms(blocked_terms)}",
        "",
    ]

    for slot in range(max_semaphores):
        enter_match = f"default(last(sem_slot_id_{slot}, sem_acquire_enter_id), -1) == " "sem_acquire_enter_id"
        exit_match = f"default(last(sem_slot_id_{slot}, sem_acquire_exit_id), -1) == " "sem_acquire_exit_id"
        timeout_match = f"default(last(sem_slot_id_{slot}, sem_timeout_id), -1) == sem_timeout_id"
        release_match = f"default(last(sem_slot_id_{slot}, sem_release_id), -1) == sem_release_id"

        range_terms["enter"].append(
            f"({enter_match} && (sem_acquire_enter_count < 0 || "
            f"sem_acquire_enter_count > default(last(sem_slot_max_{slot}, sem_acquire_enter_id), -1)))",
        )
        range_terms["exit"].append(
            f"({exit_match} && (sem_acquire_exit_count < 0 || "
            f"sem_acquire_exit_count > default(last(sem_slot_max_{slot}, sem_acquire_exit_id), -1)))",
        )
        range_terms["timeout"].append(
            f"({timeout_match} && (sem_timeout_count < 0 || "
            f"sem_timeout_count > default(last(sem_slot_max_{slot}, sem_timeout_id), -1)))",
        )
        range_terms["release"].append(
            f"({release_match} && (sem_release_count_before < 0 || "
            f"sem_release_count_after < 0 || sem_release_max_count <= 0 || "
            f"sem_release_count_before > sem_release_max_count || "
            f"sem_release_count_after > sem_release_max_count))",
        )

        parts.append(f"""def sem_count_discontinuity_enter_{slot} :=
  {enter_match} &&
  sem_acquire_enter_count != default(last(sem_slot_count_{slot}, sem_acquire_enter_id), -1)

def sem_count_discontinuity_exit_{slot} :=
  {exit_match} &&
  sem_acquire_exit_count !=
    (if sem_acquire_exit_succeeded == 1 && sem_exit_was_blocked == false
     then default(last(sem_slot_count_{slot}, sem_acquire_exit_id), -1) - 1
     else default(last(sem_slot_count_{slot}, sem_acquire_exit_id), -1))

def sem_count_discontinuity_timeout_{slot} :=
  {timeout_match} &&
  sem_timeout_count != default(last(sem_slot_count_{slot}, sem_timeout_id), -1)

def sem_count_discontinuity_release_{slot} :=
  {release_match} &&
  (sem_release_count_before != default(last(sem_slot_count_{slot}, sem_release_id), -1) ||
   sem_release_max_count != default(last(sem_slot_max_{slot}, sem_release_id), -1))

def sem_violation_count_discontinuity_enter_{slot} :=
  filter(sem_acquire_enter_id, sem_count_discontinuity_enter_{slot})

def sem_violation_count_discontinuity_exit_{slot} :=
  filter(sem_acquire_exit_id, sem_count_discontinuity_exit_{slot})

def sem_violation_count_discontinuity_timeout_{slot} :=
  filter(sem_timeout_id, sem_count_discontinuity_timeout_{slot})

def sem_violation_count_discontinuity_release_{slot} :=
  filter(sem_release_id, sem_count_discontinuity_release_{slot})
""")

        discontinuity_streams.extend(
            [
                f"sem_violation_count_discontinuity_enter_{slot}",
                f"sem_violation_count_discontinuity_exit_{slot}",
                f"sem_violation_count_discontinuity_timeout_{slot}",
                f"sem_violation_count_discontinuity_release_{slot}",
            ]
        )

    range_sources = {
        "create": "sem_create_id",
        "enter": "sem_acquire_enter_id",
        "exit": "sem_acquire_exit_id",
        "timeout": "sem_timeout_id",
        "release": "sem_release_id",
    }
    range_violation_streams = []

    for event, terms in range_terms.items():
        parts.append(f"def sem_count_out_of_range_{event} :=\n  {or_terms(terms)}")
        parts.append("")
        parts.append(f"def sem_violation_count_out_of_range_{event} :=")
        parts.append(f"  filter({range_sources[event]}, sem_count_out_of_range_{event})")
        parts.append("")
        range_violation_streams.append(f"sem_violation_count_out_of_range_{event}")

    parts.append("def violation_sem_count_out_of_range :=")
    parts.append(f"  {nested_merge(range_violation_streams, 'filter(sem_event_id, false)')}")
    parts.append("")
    parts.append("def violation_sem_count_discontinuity :=")
    parts.append(f"  {nested_merge(discontinuity_streams, 'filter(sem_event_id, false)')}")
    parts.append("")

    return "\n".join(parts)


def emit_acquire_checks(max_tasks: int, actor_ids: list[int]) -> str:
    valid_actor = " || ".join(f"sem_acquire_enter_task == {actor}" for actor in actor_ids)
    valid_exit_actor = " || ".join(f"sem_acquire_exit_task == {actor}" for actor in actor_ids)
    enter_bad_terms = [
        f"(({valid_actor}) == false)",
        "sem_acquire_enter_finite < 0 || sem_acquire_enter_finite > 1",
    ]
    exit_bad_terms = [
        f"(({valid_exit_actor}) == false)",
        "sem_acquire_exit_succeeded < 0 || sem_acquire_exit_succeeded > 1",
    ]
    empty_success_terms = []
    timeout_result_terms = []

    for actor in actor_ids:
        enter_bad_terms.append(
            f"(sem_acquire_enter_task == {actor} && "
            f"default(last(sem_acquire_active_task_{actor}, sem_acquire_enter_id), false))"
        )

        blocked = f"default(last(sem_acquire_blocked_task_{actor}, sem_acquire_exit_id), false)"
        resolution = f"default(last(sem_acquire_resolution_task_{actor}, sem_acquire_exit_id), -1)"
        common = (
            f"sem_acquire_exit_task == {actor} && ("
            f"default(last(sem_acquire_active_task_{actor}, sem_acquire_exit_id), false) == false || "
            f"default(last(sem_enter_object_task_{actor}, sem_acquire_exit_id), -1) != sem_acquire_exit_id"
        )
        result_bad = (
            f"({blocked} && {resolution} != sem_acquire_exit_succeeded) || "
            f"({blocked} == false && sem_acquire_exit_succeeded == 1 && "
            f"default(last(sem_enter_count_task_{actor}, sem_acquire_exit_id), -1) <= 0) || "
            f"({blocked} == false && sem_acquire_exit_succeeded == 0 && "
            f"default(last(sem_enter_count_task_{actor}, sem_acquire_exit_id), -1) != 0)"
        )
        exit_bad_terms.append(f"({common} || {result_bad}))")
        empty_success_terms.append(
            f"(sem_acquire_exit_task == {actor} && sem_acquire_exit_succeeded == 1 && "
            f"{blocked} == false && "
            f"default(last(sem_enter_count_task_{actor}, sem_acquire_exit_id), -1) == 0)"
        )

        if actor != TASK_ID_NONE:
            timeout_result_terms.append(
                f"(sem_acquire_exit_task == {actor} && {blocked} && {resolution} == 0 && "
                "sem_acquire_exit_succeeded != 0)"
            )

    block_bad_terms = [
        f"(sem_block_task == {task} && ("
        f"default(last(sem_acquire_active_task_{task}, sem_block_id), false) == false || "
        f"default(last(sem_acquire_blocked_task_{task}, sem_block_id), false) || "
        f"default(last(sem_enter_object_task_{task}, sem_block_id), -1) != sem_block_id || "
        f"default(last(sem_enter_count_task_{task}, sem_block_id), -1) != 0 || "
        f"default(last(sem_enter_timeout_task_{task}, sem_block_id), -1) != sem_block_timeout || "
        f"default(last(sem_enter_finite_task_{task}, sem_block_id), -1) != sem_block_finite))"
        for task in configured_task_ids(max_tasks)
    ]
    valid_block_task = " || ".join(f"sem_block_task == {task}" for task in configured_task_ids(max_tasks))

    return f"""# ---------------- Acquire checks ----------------
def sem_invalid_acquire_enter_bad :=
  {or_terms(enter_bad_terms)}

def sem_invalid_acquire_exit_bad :=
  {or_terms(exit_bad_terms)}

def sem_invalid_acquire_block_bad :=
  (({valid_block_task}) == false) ||
  sem_block_finite < 0 || sem_block_finite > 1 ||
  (sem_block_finite == 1 && sem_block_timeout <= 0) ||
  {or_terms(block_bad_terms)}

def sem_violation_invalid_acquire_enter :=
  filter(sem_acquire_enter_id, sem_invalid_acquire_enter_bad)

def sem_violation_invalid_acquire_exit :=
  filter(sem_acquire_exit_id, sem_invalid_acquire_exit_bad)

def sem_violation_invalid_acquire_block :=
  filter(sem_block_id, sem_invalid_acquire_block_bad)

def violation_sem_invalid_acquire :=
  {nested_merge([
      'sem_violation_invalid_acquire_enter',
      'sem_violation_invalid_acquire_exit',
      'sem_violation_invalid_acquire_block',
  ], 'filter(sem_event_id, false)')}

def sem_empty_acquire_succeeded_bad :=
  {or_terms(empty_success_terms)}

def violation_sem_empty_acquire_succeeded :=
  filter(sem_acquire_exit_id, sem_empty_acquire_succeeded_bad)

def sem_timeout_result_bad :=
  {or_terms(timeout_result_terms)}

def violation_sem_timeout_result :=
  filter(sem_acquire_exit_id, sem_timeout_result_bad)

"""


def emit_blocking_state_checks(max_tasks: int) -> str:
    task_match = [
        f"(state_id == {task} && "
        f"default(last(sem_block_task, state_id), -1) == {task})"
        for task in configured_task_ids(max_tasks)
    ]
    unconfirmed_wake = []
    unconfirmed_timeout = []

    for task in configured_task_ids(max_tasks):
        unconfirmed_wake.append(
            f"(sem_wake_task == {task} && " f"default(last(sem_block_unconfirmed_task_{task}, sem_wake_id), false))"
        )
        unconfirmed_timeout.append(
            f"(sem_timeout_task == {task} && "
            f"default(last(sem_block_unconfirmed_task_{task}, sem_timeout_id), false))"
        )

    return f"""# ---------------- Blocking-state checks ----------------
def sem_first_state_after_block :=
  default(last(sem_state_sequence_at_block, state_id), -1) ==
  default(last(sem_state_sequence, state_id), 0)

def sem_blocking_state_bad :=
  sem_first_state_after_block &&
  (({or_terms(task_match)}) == false || state_new != {STATE_BLOCKED})

def sem_violation_blocking_state_transition :=
  filter(state_id, sem_blocking_state_bad)

def sem_wake_without_blocked_state :=
  {or_terms(unconfirmed_wake)}

def sem_timeout_without_blocked_state :=
  {or_terms(unconfirmed_timeout)}

def sem_violation_wake_without_blocked_state :=
  filter(sem_wake_id, sem_wake_without_blocked_state)

def sem_violation_timeout_without_blocked_state :=
  filter(sem_timeout_id, sem_timeout_without_blocked_state)

def violation_sem_blocking_state :=
  merge(sem_violation_blocking_state_transition,
        merge(sem_violation_wake_without_blocked_state,
              sem_violation_timeout_without_blocked_state))

"""


def emit_timeout_checks(max_tasks: int) -> str:
    early_terms = []
    lifecycle_terms = []
    valid_task_terms = [f"sem_timeout_task == {task}" for task in configured_task_ids(max_tasks)]

    for task in configured_task_ids(max_tasks):
        is_task = f"sem_timeout_task == {task}"
        lifecycle_terms.append(
            f"({is_task} && ("
            f"default(last(sem_waiting_task_{task}, sem_timeout_id), false) == false || "
            f"default(last(sem_wait_object_task_{task}, sem_timeout_id), -1) != sem_timeout_id || "
            f"default(last(sem_wait_finite_task_{task}, sem_timeout_id), 0) != 1))"
        )
        early_terms.append(
            f"({is_task} && "
            f"default(last(sem_tick_sum, sem_timeout_id), 0) - "
            f"default(last(sem_wait_start_tick_task_{task}, sem_timeout_id), 0) < "
            f"default(last(sem_wait_timeout_task_{task}, sem_timeout_id), 0))"
        )

    return f"""# ---------------- Timeout checks ----------------
def sem_timeout_lifecycle_bad :=
  (({or_terms(valid_task_terms)}) == false) ||
  {or_terms(lifecycle_terms)}

def sem_timeout_too_early_bad :=
  {or_terms(early_terms)}

def sem_violation_timeout_lifecycle :=
  filter(sem_timeout_id, sem_timeout_lifecycle_bad)

def violation_sem_timeout_too_early :=
  filter(sem_timeout_id, sem_timeout_too_early_bad)

"""


def emit_release_and_wake_checks(max_tasks: int) -> str:
    waiter_terms = waiting_on_release_terms(max_tasks)
    valid_wake_task = [f"sem_wake_task == {task}" for task in configured_task_ids(max_tasks)]
    wake_lifecycle_terms = []
    priority_terms = []
    fifo_terms = []

    for chosen in configured_task_ids(max_tasks):
        chosen_waiting = f"default(last(sem_waiting_task_{chosen}, sem_wake_id), false)"
        chosen_object = f"default(last(sem_wait_object_task_{chosen}, sem_wake_id), -1)"
        chosen_priority = f"default(last(sem_wait_priority_task_{chosen}, sem_wake_id), -1)"
        chosen_order = f"default(last(sem_wait_order_task_{chosen}, sem_wake_id), -1)"

        wake_lifecycle_terms.append(
            f"(sem_wake_task == {chosen} && ("
            f"{chosen_waiting} == false || {chosen_object} != sem_wake_id || "
            f"{chosen_priority} != sem_wake_prio))"
        )

        for other in configured_task_ids(max_tasks):
            if chosen == other:
                continue

            other_waiting = f"default(last(sem_waiting_task_{other}, sem_wake_id), false)"
            other_object = f"default(last(sem_wait_object_task_{other}, sem_wake_id), -1)"
            other_priority = f"default(last(sem_wait_priority_task_{other}, sem_wake_id), -1)"
            other_order = f"default(last(sem_wait_order_task_{other}, sem_wake_id), -1)"

            priority_terms.append(
                f"(sem_wake_task == {chosen} && {other_waiting} && "
                f"{other_object} == sem_wake_id && {other_priority} > {chosen_priority})"
            )
            fifo_terms.append(
                f"(sem_wake_task == {chosen} && {other_waiting} && "
                f"{other_object} == sem_wake_id && {other_priority} == {chosen_priority} && "
                f"{other_order} < {chosen_order})"
            )

    return f"""# ---------------- Release and wake checks ----------------
def sem_release_has_waiter :=
  {or_terms(waiter_terms)}

def sem_release_requires_wake :=
  sem_release_succeeded == 1 && sem_release_has_waiter

def sem_invalid_release_bad :=
  sem_release_succeeded < 0 || sem_release_succeeded > 1 ||
  (sem_release_has_waiter &&
   (sem_release_succeeded != 1 || sem_release_count_after != sem_release_count_before)) ||
  (sem_release_has_waiter == false && sem_release_count_before < sem_release_max_count &&
   (sem_release_succeeded != 1 || sem_release_count_after != sem_release_count_before + 1)) ||
  (sem_release_has_waiter == false && sem_release_count_before >= sem_release_max_count &&
   (sem_release_succeeded != 0 || sem_release_count_after != sem_release_count_before))

def violation_sem_invalid_release :=
  filter(sem_release_id, sem_invalid_release_bad)

def sem_previous_event_required_wake :=
  default(last(sem_event_kind, sem_event_kind), 0) == {KIND_RELEASE} &&
  default(last(sem_release_requires_wake, sem_event_kind), false)

def sem_missing_wake_bad :=
  sem_previous_event_required_wake && sem_event_kind != {KIND_WAKE}

def violation_sem_missing_wake :=
  filter(sem_event_id, sem_missing_wake_bad)

def sem_wake_not_after_release :=
  default(last(sem_event_kind, sem_wake_id), 0) != {KIND_RELEASE} ||
  default(last(sem_release_requires_wake, sem_wake_id), false) == false ||
  default(last(sem_release_id, sem_wake_id), -1) != sem_wake_id

def sem_wake_lifecycle_bad :=
  (({or_terms(valid_wake_task)}) == false) ||
  sem_wake_not_after_release ||
  {or_terms(wake_lifecycle_terms)}

def sem_violation_wake_lifecycle :=
  filter(sem_wake_id, sem_wake_lifecycle_bad)

def sem_wake_priority_bad :=
  {or_terms(priority_terms)}

def violation_sem_wake_priority :=
  filter(sem_wake_id, sem_wake_priority_bad)

def sem_wake_fifo_bad :=
  {or_terms(fifo_terms)}

def violation_sem_wake_fifo :=
  filter(sem_wake_id, sem_wake_fifo_bad)

def violation_sem_invalid_wait_lifecycle :=
  merge(sem_violation_timeout_lifecycle, sem_violation_wake_lifecycle)

"""


def emit_outputs(mode: str) -> str:
    lines = []

    if mode == "violations":
        for _, internal_name, _ in CHECKS:
            lines.append(f"out violation_{internal_name}")

    elif mode == "checks":
        for public_name, internal_name, trigger in CHECKS:
            lines.append(f"def FAIL_{public_name} := violation_{internal_name}")
            lines.append(
                f"def sem_last_failure_time_{internal_name} := "
                f"merge(time(violation_{internal_name}), "
                f"default(last(time(violation_{internal_name}), {trigger}), -1))"
            )
            lines.append(
                f"def PASS_{public_name} := filter({trigger}, "
                f"time({trigger}) != sem_last_failure_time_{internal_name})"
            )
            lines.append(f"out FAIL_{public_name}")
            lines.append(f"out PASS_{public_name}")

    else:
        raise ValueError(f"invalid mode: {mode}")

    return "\n".join(lines) + "\n"


def generate(
    max_tasks: int,
    max_semaphores: int,
    mode: str = "violations",
) -> str:
    if max_tasks <= 0 or max_tasks >= TASK_ID_NONE:
        raise ValueError("max_tasks must be in the range 1..254")

    if max_semaphores <= 0:
        raise ValueError("max_semaphores must be greater than 0")

    task_ids = list(configured_task_ids(max_tasks))
    actor_ids = [*task_ids, TASK_ID_NONE]


    parts = [
        emit_header(),
        emit_event_model(),
    ]

    for task_id in task_ids:
        parts.append(emit_task_model(task_id))

    parts.append(emit_taskless_actor_model())
    parts.append(emit_slot_assignment(max_semaphores))

    for slot in range(max_semaphores):
        parts.append(emit_slot_model(slot))

    parts.extend(
        [
            emit_create_checks(max_tasks),
            emit_count_checks(max_semaphores, actor_ids),
            emit_acquire_checks(max_tasks, actor_ids),
            emit_blocking_state_checks(max_tasks),
            emit_timeout_checks(max_tasks),
            emit_release_and_wake_checks(max_tasks),
            emit_outputs(mode),
        ]
    )

    return "\n".join(parts)


def main() -> int:
    import argparse

    parser = argparse.ArgumentParser(description="Generate the semaphore TeSSLa monitor.")
    parser.add_argument("--max-tasks", type=int, default=3)
    parser.add_argument("--max-semaphores", type=int, default=2)
    parser.add_argument("--mode", choices=["violations", "checks"], default="violations")
    args = parser.parse_args()

    print(generate(args.max_tasks, args.max_semaphores, args.mode), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
