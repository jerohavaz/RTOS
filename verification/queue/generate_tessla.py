"""Generate the message-queue TeSSLa verification monitor.

Author: Jerome
"""

from typing import Mapping

CHECKS = [
    "queue_configuration",
    "queue_fill_bounds",
    "queue_fill_consistency",
    "direct_send_consistency",
    "direct_receive_consistency",
    "write_to_full_queue",
    "read_from_empty_queue",
    "fifo_order",
    "message_integrity",
    "fifo_model_bounds",
    "queue_block_state_transition",
    "queue_blocked_event",
    "queue_block_state_missing",
    "queue_blocked_event_missing",
    "queue_wake_state_transition",
    "queue_ready_event",
    "queue_wake_state_missing",
    "queue_ready_event_missing",
    "waiting_sender_not_woken",
    "waiting_receiver_not_handed_off",
    "waiting_receiver_not_woken",
    "receive_timeout_too_early",
    "send_timeout_too_early",
    "no_wait_send_blocked",
    "no_wait_receive_blocked",
    "no_wait_send_timed_out",
    "no_wait_receive_timed_out",
    "wait_forever_send_timed_out",
    "wait_forever_receive_timed_out",
]

GLOBAL_CHECKS = [
    "untracked_queue",
]


QUEUE_ID_STREAMS = [
    "queue_create_id",
    "queue_send_attempt_queue_id",
    "queue_send_success_queue_id",
    "queue_send_block_queue_id",
    "queue_send_timeout_queue_id",
    "queue_recv_attempt_queue_id",
    "queue_recv_success_queue_id",
    "queue_recv_block_queue_id",
    "queue_recv_timeout_queue_id",
    "queue_wake_send_queue_id",
    "queue_wake_recv_queue_id",
    "queue_handoff_queue_id",
    "queue_fill_queue_id",
]


CHECK_TRIGGERS = {
    "queue_configuration": "queue_create_id >= 0",
    "queue_fill_bounds": "queue_fill_queue_id >= 0",
    "queue_fill_consistency": (
        "merge(queue_send_success_queue_id, " "merge(queue_recv_success_queue_id, queue_handoff_queue_id)) >= 0"
    ),
    "direct_send_consistency": "queue_send_success_queue_id >= 0",
    "direct_receive_consistency": "queue_recv_success_queue_id >= 0",
    "write_to_full_queue": "queue_send_success_queue_id >= 0",
    "read_from_empty_queue": "queue_recv_success_queue_id >= 0",
    "fifo_order": "queue_recv_success_queue_id >= 0",
    "message_integrity": "queue_recv_success_queue_id >= 0",
    "fifo_model_bounds": ("merge(queue_send_success_task_id, queue_recv_success_task_id) >= 0"),
    "queue_block_state_transition": "state_id >= 0",
    "queue_blocked_event": "blocked_id >= 0",
    "queue_block_state_missing": "tick > 0",
    "queue_blocked_event_missing": "tick > 0",
    "queue_wake_state_transition": "state_id >= 0",
    "queue_ready_event": "ready_id >= 0",
    "queue_wake_state_missing": "tick > 0",
    "queue_ready_event_missing": "tick > 0",
    "waiting_sender_not_woken": "tick > 0",
    "waiting_receiver_not_handed_off": ("merge(queue_send_success_queue_id, tick) >= 0"),
    "waiting_receiver_not_woken": "tick > 0",
    "receive_timeout_too_early": "queue_recv_timeout_queue_id >= 0",
    "send_timeout_too_early": "queue_send_timeout_queue_id >= 0",
    "no_wait_send_blocked": "queue_send_block_queue_id >= 0",
    "no_wait_receive_blocked": "queue_recv_block_queue_id >= 0",
    "no_wait_send_timed_out": "queue_send_timeout_queue_id >= 0",
    "no_wait_receive_timed_out": "queue_recv_timeout_queue_id >= 0",
    "wait_forever_send_timed_out": "queue_send_timeout_queue_id >= 0",
    "wait_forever_receive_timed_out": "queue_recv_timeout_queue_id >= 0",
    "untracked_queue": "queue_event_id >= 0",
}


def emit_header() -> str:
    """Emit the queue monitor documentation and input declarations."""
    return """# Module: queue
# Purpose: Verify queue capacity, FIFO data, blocking, handoff, and timeouts.
# Generator: queue/generate_tessla.py
# Author: Jerome

in task_create_id: Events[Int]
in task_create_prio: Events[Int]

in state_id: Events[Int]
in state_old: Events[Int]
in state_new: Events[Int]
in ready_id: Events[Int]
in ready_prio: Events[Int]
in blocked_id: Events[Int]
in tick: Events[Int]

in queue_create_id: Events[Int]
in queue_create_capacity: Events[Int]
in queue_send_attempt_queue_id: Events[Int]
in queue_send_attempt_task_id: Events[Int]
in queue_send_attempt_task_prio: Events[Int]
in queue_send_attempt_timeout: Events[Int]
in queue_send_attempt_hash: Events[Int]
in queue_send_success_queue_id: Events[Int]
in queue_send_success_task_id: Events[Int]
in queue_send_success_hash: Events[Int]
in queue_send_block_queue_id: Events[Int]
in queue_send_block_task_id: Events[Int]
in queue_send_block_task_prio: Events[Int]
in queue_send_timeout_queue_id: Events[Int]
in queue_send_timeout_task_id: Events[Int]
in queue_recv_attempt_queue_id: Events[Int]
in queue_recv_attempt_task_id: Events[Int]
in queue_recv_attempt_task_prio: Events[Int]
in queue_recv_attempt_timeout: Events[Int]
in queue_recv_success_queue_id: Events[Int]
in queue_recv_success_task_id: Events[Int]
in queue_recv_success_hash: Events[Int]
in queue_recv_block_queue_id: Events[Int]
in queue_recv_block_task_id: Events[Int]
in queue_recv_block_task_prio: Events[Int]
in queue_recv_timeout_queue_id: Events[Int]
in queue_recv_timeout_task_id: Events[Int]
in queue_wake_send_queue_id: Events[Int]
in queue_wake_send_task_id: Events[Int]
in queue_wake_recv_queue_id: Events[Int]
in queue_wake_recv_task_id: Events[Int]
in queue_handoff_queue_id: Events[Int]
in queue_handoff_sender_id: Events[Int]
in queue_handoff_receiver_id: Events[Int]
in queue_handoff_hash: Events[Int]
in queue_fill_queue_id: Events[Int]
in queue_fill_value: Events[Int]

def os_ticks: Events[Int] =
  merge(
    if tick > 0 then last(os_ticks, tick) + tick
    else last(os_ticks, tick),
    0
  )

"""


def or_terms(terms: list[str], indent: str = "  ") -> str:
    """Join TeSSLa Boolean expressions with a formatted logical OR."""
    if not terms:
        return "false"
    return (" ||\n" + indent).join(terms)


def merge_streams(streams: list[str], default: str | None = None) -> str:
    """Build a nested binary merge expression from stream names."""
    items = list(streams)
    if default is not None:
        items.append(default)
    if not items:
        raise ValueError("cannot merge an empty stream list")
    expression = items[-1]
    for stream in reversed(items[:-1]):
        expression = f"merge({stream}, {expression})"
    return expression


def emit_pass_fail_pair(
    public_name: str,
    trigger_expr: str,
) -> str:
    """Emit one checks-mode PASS/FAIL pair for a queue property."""
    marker_name = f"{public_name}_check_marker"
    violation_name = f"violation_{public_name}"
    fail_condition = f"merge({violation_name} == {violation_name}, " f"{marker_name} != {marker_name})"

    return f"""def {marker_name} :=
  if {trigger_expr} then 1
  else 0

def FAIL_{public_name} :=
  filter({marker_name}, {fail_condition})

def PASS_{public_name} :=
  filter({marker_name}, {fail_condition} == false)

"""


def emit_wait_state(queue_id: int, task_id: int, kind: str) -> str:
    """Emit per-task send or receive waiter-state tracking."""
    q = queue_id
    t = task_id
    if kind == "send":
        block_queue = "queue_send_block_queue_id"
        block_task = "queue_send_block_task_id"
        wake_queue = "queue_wake_send_queue_id"
        wake_task = "queue_wake_send_task_id"
        timeout_queue = "queue_send_timeout_queue_id"
        timeout_task = "queue_send_timeout_task_id"
    else:
        block_queue = "queue_recv_block_queue_id"
        block_task = "queue_recv_block_task_id"
        wake_queue = "queue_wake_recv_queue_id"
        wake_task = "queue_wake_recv_task_id"
        timeout_queue = "queue_recv_timeout_queue_id"
        timeout_task = "queue_recv_timeout_task_id"

    return f"""def {kind}_wait_start_q{q}_t{t} :=
  filter({block_task}, {block_queue} == {q} && {block_task} == {t}) >= 0

def {kind}_wait_stop_q{q}_t{t} :=
  merge(
    filter({wake_task}, {wake_queue} == {q} && {wake_task} == {t}),
    filter({timeout_task}, {timeout_queue} == {q} && {timeout_task} == {t})
  ) >= 0

def {kind}_wait_q{q}_t{t}: Events[Bool] =
  merge(
    {kind}_wait_start_q{q}_t{t},
    merge(if {kind}_wait_stop_q{q}_t{t} then false else true, false)
  )

"""


def emit_timeout_task(queue_id: int, task_id: int, kind: str) -> str:
    """Emit per-task timeout request and block-time tracking."""
    q = queue_id
    t = task_id
    if kind == "send":
        attempt_queue = "queue_send_attempt_queue_id"
        attempt_task = "queue_send_attempt_task_id"
        attempt_timeout = "queue_send_attempt_timeout"
        block_queue = "queue_send_block_queue_id"
        block_task = "queue_send_block_task_id"
    else:
        attempt_queue = "queue_recv_attempt_queue_id"
        attempt_task = "queue_recv_attempt_task_id"
        attempt_timeout = "queue_recv_attempt_timeout"
        block_queue = "queue_recv_block_queue_id"
        block_task = "queue_recv_block_task_id"

    return f"""def {kind}_attempt_timeout_q{q}_t{t}: Events[Int] =
  merge(
    filter(
      {attempt_timeout},
      {attempt_queue} == {q} && {attempt_task} == {t}
    ),
    -1
  )

def {kind}_block_tick_q{q}_t{t}: Events[Int] =
  merge(
    if {block_queue} == {q} && {block_task} == {t} &&
       last({kind}_attempt_timeout_q{q}_t{t}, {block_task}) > 0 &&
       last({kind}_attempt_timeout_q{q}_t{t}, {block_task}) != 4294967295
      then last(os_ticks, {block_task})
    else -1,
    -1
  )

"""


def emit_fifo_slots(queue_id: int, capacity: int) -> str:
    """Emit the bounded FIFO payload model for one configured queue."""
    q = queue_id
    parts = []
    for slot in range(capacity):
        shift = f"last(fifo_slot_q{q}_{slot + 1}, fifo_delta_q{q})" if slot + 1 < capacity else "-1"
        parts.append(f"""def fifo_slot_q{q}_{slot}: Events[Int] =
  merge(
    if fifo_delta_q{q} == 1 && last(fifo_count_q{q}, fifo_delta_q{q}) == {slot}
      then fifo_operation_hash_q{q}
    else if fifo_delta_q{q} == -1 &&
            last(fifo_count_q{q}, fifo_delta_q{q}) > {slot + 1}
      then {shift}
    else if fifo_delta_q{q} == -1 then -1
    else if fifo_delta_q{q} == 1 then last(fifo_slot_q{q}_{slot}, fifo_delta_q{q})
    else -1,
    -1
  )

""")
    return "".join(parts)


def emit_queue(queue_id: int, capacity: int, max_tasks: int) -> str:
    """Emit all per-queue state and property checks."""
    q = queue_id
    wait_states = "".join(
        emit_wait_state(q, task_id, kind) for task_id in range(max_tasks) for kind in ("send", "recv")
    )
    timeout_states = "".join(
        emit_timeout_task(q, task_id, kind) for task_id in range(max_tasks) for kind in ("send", "recv")
    )
    waiting_senders = or_terms(
        [f"last(send_wait_q{q}_t{task}, queue_recv_success_hash)" for task in range(max_tasks)],
        "   ",
    )
    waiting_receivers_at_send = or_terms(
        [f"last(recv_wait_q{q}_t{task}, queue_send_attempt_queue_id)" for task in range(max_tasks)],
        "   ",
    )
    handoff_receiver_waiting = or_terms(
        [
            f"(queue_handoff_receiver_id == {task} && " f"last(recv_wait_q{q}_t{task}, queue_handoff_receiver_id))"
            for task in range(max_tasks)
        ],
        "   ",
    )
    fifo_later_terms = [
        f"(last(fifo_count_q{q}, queue_recv_success_hash) > {slot} && "
        f"queue_recv_success_hash == last(fifo_slot_q{q}_{slot}, queue_recv_success_hash))"
        for slot in range(1, capacity)
    ]
    fifo_later_match = or_terms(fifo_later_terms, "   ")
    send_early_terms = []
    recv_early_terms = []
    no_wait_send_terms = []
    no_wait_recv_terms = []
    forever_send_terms = []
    forever_recv_terms = []
    no_wait_send_timeout_terms = []
    no_wait_recv_timeout_terms = []
    for task in range(max_tasks):
        send_early_terms.append(
            f"(queue_send_timeout_task_id == {task} && "
            f"last(send_block_tick_q{q}_t{task}, queue_send_timeout_task_id) >= 0 && "
            f"last(os_ticks, queue_send_timeout_task_id) - "
            f"last(send_block_tick_q{q}_t{task}, queue_send_timeout_task_id) < "
            f"last(send_attempt_timeout_q{q}_t{task}, queue_send_timeout_task_id))"
        )
        recv_early_terms.append(
            f"(queue_recv_timeout_task_id == {task} && "
            f"last(recv_block_tick_q{q}_t{task}, queue_recv_timeout_task_id) >= 0 && "
            f"last(os_ticks, queue_recv_timeout_task_id) - "
            f"last(recv_block_tick_q{q}_t{task}, queue_recv_timeout_task_id) < "
            f"last(recv_attempt_timeout_q{q}_t{task}, queue_recv_timeout_task_id))"
        )
        no_wait_send_terms.append(
            f"(queue_send_block_task_id == {task} && "
            f"last(send_attempt_timeout_q{q}_t{task}, queue_send_block_task_id) == 0)"
        )
        no_wait_recv_terms.append(
            f"(queue_recv_block_task_id == {task} && "
            f"last(recv_attempt_timeout_q{q}_t{task}, queue_recv_block_task_id) == 0)"
        )
        forever_send_terms.append(
            f"(queue_send_timeout_task_id == {task} && "
            f"last(send_attempt_timeout_q{q}_t{task}, queue_send_timeout_task_id) == 4294967295)"
        )
        forever_recv_terms.append(
            f"(queue_recv_timeout_task_id == {task} && "
            f"last(recv_attempt_timeout_q{q}_t{task}, queue_recv_timeout_task_id) == 4294967295)"
        )
        no_wait_send_timeout_terms.append(
            f"(queue_send_timeout_task_id == {task} && "
            f"last(send_attempt_timeout_q{q}_t{task}, queue_send_timeout_task_id) == 0)"
        )
        no_wait_recv_timeout_terms.append(
            f"(queue_recv_timeout_task_id == {task} && "
            f"last(recv_attempt_timeout_q{q}_t{task}, queue_recv_timeout_task_id) == 0)"
        )

    return f"""def queue_created_q{q} :=
  queue_create_id == {q}

def violation_queue_configuration_q{q} :=
  filter(queue_create_id, queue_created_q{q} && queue_create_capacity != {capacity})

def queue_fill_q{q} :=
  filter(queue_fill_value, queue_fill_queue_id == {q})

def fill_delta_q{q} :=
  queue_fill_q{q} - last(queue_fill_q{q}, queue_fill_q{q})

def violation_queue_fill_bounds_q{q} :=
  filter(
    queue_fill_q{q},
    queue_fill_q{q} < 0 || queue_fill_q{q} > {capacity} ||
    fill_delta_q{q} < -1 || fill_delta_q{q} > 1
  )

def handoff_time_q{q}: Events[Int] =
  merge(time(filter(queue_handoff_queue_id, queue_handoff_queue_id == {q})), -100)

def handoff_sender_q{q}: Events[Int] =
  merge(filter(queue_handoff_sender_id, queue_handoff_queue_id == {q}), -1)

def handoff_receiver_q{q}: Events[Int] =
  merge(filter(queue_handoff_receiver_id, queue_handoff_queue_id == {q}), -1)

def handoff_message_q{q}: Events[Int] =
  merge(filter(queue_handoff_hash, queue_handoff_queue_id == {q}), -1)

def receiver_wake_time_q{q}: Events[Int] =
  merge(time(filter(queue_wake_recv_task_id, queue_wake_recv_queue_id == {q})), -100)

def sender_wake_time_q{q}: Events[Int] =
  merge(time(filter(queue_wake_send_task_id, queue_wake_send_queue_id == {q})), -100)

def direct_send_q{q} :=
  queue_send_success_queue_id == {q} &&
  last(handoff_time_q{q}, queue_send_success_queue_id) + 1 == time(queue_send_success_queue_id)

def direct_recv_q{q} :=
  queue_recv_success_queue_id == {q} &&
  last(handoff_time_q{q}, queue_recv_success_queue_id) + 2 == time(queue_recv_success_queue_id)

def buffered_send_q{q} :=
  queue_send_success_queue_id == {q} && direct_send_q{q} == false

def buffered_recv_q{q} :=
  queue_recv_success_queue_id == {q} && direct_recv_q{q} == false

def violation_direct_send_consistency_q{q} :=
  filter(
    queue_send_success_task_id,
    direct_send_q{q} &&
    (queue_send_success_task_id != last(handoff_sender_q{q}, queue_send_success_task_id) ||
     queue_send_success_hash != last(handoff_message_q{q}, queue_send_success_task_id))
  )

def violation_direct_receive_consistency_q{q} :=
  filter(
    queue_recv_success_task_id,
    direct_recv_q{q} &&
    (queue_recv_success_task_id != last(handoff_receiver_q{q}, queue_recv_success_task_id) ||
     queue_recv_success_hash != last(handoff_message_q{q}, queue_recv_success_task_id))
  )

def fifo_send_delta_q{q} :=
  if buffered_send_q{q} then 1 else 0

def fifo_recv_delta_q{q} :=
  if buffered_recv_q{q} then -1 else 0

def fifo_delta_q{q} :=
  merge(fifo_send_delta_q{q}, fifo_recv_delta_q{q})

def fifo_operation_hash_q{q} :=
  merge(
    filter(queue_send_success_hash, buffered_send_q{q}),
    filter(queue_recv_success_hash, buffered_recv_q{q})
  )

def fifo_count_q{q}: Events[Int] =
  merge(last(fifo_count_q{q}, fifo_delta_q{q}) + fifo_delta_q{q}, 0)

def violation_queue_fill_consistency_send_q{q} :=
  filter(
    queue_send_success_task_id,
    buffered_send_q{q} &&
    (default(last(time(queue_fill_q{q}), queue_send_success_task_id), -2) + 1 !=
       time(queue_send_success_task_id) ||
     default(last(queue_fill_q{q}, queue_send_success_task_id), -1) !=
       default(last(fifo_count_q{q}, queue_send_success_task_id), 0) + 1)
  )

def violation_queue_fill_consistency_receive_q{q} :=
  filter(
    queue_recv_success_task_id,
    buffered_recv_q{q} &&
    (default(last(time(queue_fill_q{q}), queue_recv_success_task_id), -2) + 1 !=
       time(queue_recv_success_task_id) ||
     default(last(queue_fill_q{q}, queue_recv_success_task_id), -1) !=
       default(last(fifo_count_q{q}, queue_recv_success_task_id), 0) - 1)
  )

def violation_queue_fill_consistency_handoff_q{q} :=
  filter(
    queue_handoff_queue_id,
    queue_handoff_queue_id == {q} &&
    default(last(queue_fill_q{q}, queue_handoff_queue_id), -1) !=
      default(last(fifo_count_q{q}, queue_handoff_queue_id), 0)
  )

def violation_queue_fill_consistency_q{q} :=
  merge(violation_queue_fill_consistency_send_q{q},
        merge(violation_queue_fill_consistency_receive_q{q},
              violation_queue_fill_consistency_handoff_q{q}))

{emit_fifo_slots(q, capacity)}def fifo_later_match_q{q} :=
  {fifo_later_match}

def violation_write_to_full_queue_q{q} :=
  filter(
    queue_send_success_task_id,
    buffered_send_q{q} && last(fifo_count_q{q}, queue_send_success_hash) >= {capacity}
  )

def violation_read_from_empty_queue_q{q} :=
  filter(
    queue_recv_success_task_id,
    buffered_recv_q{q} && last(fifo_count_q{q}, queue_recv_success_hash) <= 0
  )

def violation_fifo_order_q{q} :=
  filter(
    queue_recv_success_task_id,
    buffered_recv_q{q} &&
    last(fifo_count_q{q}, queue_recv_success_hash) > 0 &&
    queue_recv_success_hash != last(fifo_slot_q{q}_0, queue_recv_success_hash) &&
    fifo_later_match_q{q}
  )

def violation_message_integrity_q{q} :=
  filter(
    queue_recv_success_task_id,
    buffered_recv_q{q} &&
    last(fifo_count_q{q}, queue_recv_success_hash) > 0 &&
    queue_recv_success_hash != last(fifo_slot_q{q}_0, queue_recv_success_hash) &&
    fifo_later_match_q{q} == false
  )

def violation_fifo_model_bounds_q{q} :=
  filter(fifo_count_q{q}, fifo_count_q{q} < 0 || fifo_count_q{q} > {capacity})

def expected_block_task_q{q}: Events[Int] =
  merge(
    filter(queue_send_block_task_id, queue_send_block_queue_id == {q}),
    merge(filter(queue_recv_block_task_id, queue_recv_block_queue_id == {q}), -1)
  )

def block_state_bad_q{q} :=
  last(time(expected_block_task_q{q}), state_id) + 1 == time(state_id) &&
  (state_id != last(expected_block_task_q{q}, state_id) || state_old != 2 || state_new != 3)

def valid_queue_block_state_q{q}: Events[Int] =
  merge(
    filter(
      state_id,
      last(time(expected_block_task_q{q}), state_id) + 1 == time(state_id) &&
      state_id == last(expected_block_task_q{q}, state_id) && state_old == 2 && state_new == 3
    ),
    -1
  )

def valid_queue_blocked_event_q{q}: Events[Int] =
  merge(
    filter(
      blocked_id,
      last(time(valid_queue_block_state_q{q}), blocked_id) + 1 == time(blocked_id) &&
      blocked_id == last(expected_block_task_q{q}, blocked_id)
    ),
    -1
  )

def violation_queue_block_state_transition_q{q} :=
  filter(state_id, block_state_bad_q{q})

def violation_queue_blocked_event_q{q} :=
  filter(
    blocked_id,
    last(time(valid_queue_block_state_q{q}), blocked_id) + 1 == time(blocked_id) &&
    blocked_id != last(expected_block_task_q{q}, blocked_id)
  )

def violation_queue_block_state_missing_q{q} :=
  filter(tick, last(time(expected_block_task_q{q}), tick) > last(time(valid_queue_block_state_q{q}), tick))

def violation_queue_blocked_event_missing_q{q} :=
  filter(tick, last(time(valid_queue_block_state_q{q}), tick) > last(time(valid_queue_blocked_event_q{q}), tick))

def expected_ready_task_q{q}: Events[Int] =
  merge(
    filter(queue_wake_send_task_id, queue_wake_send_queue_id == {q}),
    merge(filter(queue_wake_recv_task_id, queue_wake_recv_queue_id == {q}), -1)
  )

def wake_state_bad_q{q} :=
  last(time(expected_ready_task_q{q}), state_id) + 1 == time(state_id) &&
  (state_id != last(expected_ready_task_q{q}, state_id) || state_old != 3 || state_new != 1)

def valid_queue_wake_state_q{q}: Events[Int] =
  merge(
    filter(
      state_id,
      last(time(expected_ready_task_q{q}), state_id) + 1 == time(state_id) &&
      state_id == last(expected_ready_task_q{q}, state_id) && state_old == 3 && state_new == 1
    ),
    -1
  )

def valid_queue_ready_event_q{q}: Events[Int] =
  merge(
    filter(
      ready_id,
      last(time(valid_queue_wake_state_q{q}), ready_id) + 1 == time(ready_id) &&
      ready_id == last(expected_ready_task_q{q}, ready_id)
    ),
    -1
  )

def violation_queue_wake_state_transition_q{q} :=
  filter(state_id, wake_state_bad_q{q})

def violation_queue_ready_event_q{q} :=
  filter(
    ready_id,
    last(time(valid_queue_wake_state_q{q}), ready_id) + 1 == time(ready_id) &&
    ready_id != last(expected_ready_task_q{q}, ready_id)
  )

def violation_queue_wake_state_missing_q{q} :=
  filter(tick, last(time(expected_ready_task_q{q}), tick) > last(time(valid_queue_wake_state_q{q}), tick))

def violation_queue_ready_event_missing_q{q} :=
  filter(tick, last(time(valid_queue_wake_state_q{q}), tick) > last(time(valid_queue_ready_event_q{q}), tick))

{wait_states}def waiting_sender_at_receive_q{q} :=
  buffered_recv_q{q} &&
  ({waiting_senders})

def sender_wake_requirement_time_q{q}: Events[Int] =
  merge(time(filter(queue_recv_success_task_id, waiting_sender_at_receive_q{q})), -100)

def waiting_receiver_at_send_q{q} :=
  queue_send_attempt_queue_id == {q} &&
  ({waiting_receivers_at_send})

def receiver_handoff_requirement_time_q{q}: Events[Int] =
  merge(time(filter(queue_send_attempt_queue_id, waiting_receiver_at_send_q{q})), -100)

def receiver_handoff_required_sender_q{q}: Events[Int] =
  merge(filter(queue_send_attempt_task_id, waiting_receiver_at_send_q{q}), -1)

def receiver_handoff_matches_requirement_q{q} :=
  queue_handoff_queue_id == {q} &&
  ({handoff_receiver_waiting}) &&
  queue_handoff_sender_id ==
    default(last(receiver_handoff_required_sender_q{q}, queue_handoff_queue_id), -1)

def matching_receiver_handoff_time_q{q}: Events[Int] =
  merge(time(filter(queue_handoff_queue_id, receiver_handoff_matches_requirement_q{q})), -100)

def receiver_wake_requirement_time_q{q}: Events[Int] =
  merge(
    time(filter(queue_handoff_queue_id,
                queue_handoff_queue_id == {q} && ({handoff_receiver_waiting}))),
    -100
  )

def violation_waiting_sender_not_woken_q{q} :=
  filter(tick, last(sender_wake_requirement_time_q{q}, tick) > last(sender_wake_time_q{q}, tick))

def receiver_handoff_missing_at_send_success_q{q} :=
  queue_send_success_queue_id == {q} &&
  default(last(receiver_handoff_requirement_time_q{q}, queue_send_success_queue_id), -100) >
    default(last(matching_receiver_handoff_time_q{q}, queue_send_success_queue_id), -100)

def receiver_handoff_missing_at_tick_q{q} :=
  default(last(receiver_handoff_requirement_time_q{q}, tick), -100) >
    default(last(matching_receiver_handoff_time_q{q}, tick), -100)

def violation_waiting_receiver_not_handed_off_q{q} :=
  merge(
    filter(queue_send_success_queue_id, receiver_handoff_missing_at_send_success_q{q}),
    filter(tick, receiver_handoff_missing_at_tick_q{q})
  )

def violation_waiting_receiver_not_woken_q{q} :=
  filter(tick,
    last(receiver_wake_requirement_time_q{q}, tick) > last(receiver_wake_time_q{q}, tick))

{timeout_states}def violation_send_timeout_too_early_q{q} :=
  filter(
    queue_send_timeout_task_id,
    queue_send_timeout_queue_id == {q} &&
    ({or_terms(send_early_terms, '     ')})
  )

def violation_receive_timeout_too_early_q{q} :=
  filter(
    queue_recv_timeout_task_id,
    queue_recv_timeout_queue_id == {q} &&
    ({or_terms(recv_early_terms, '     ')})
  )

def violation_no_wait_send_blocked_q{q} :=
  filter(
    queue_send_block_task_id,
    queue_send_block_queue_id == {q} && ({or_terms(no_wait_send_terms, '     ')})
  )

def violation_no_wait_receive_blocked_q{q} :=
  filter(
    queue_recv_block_task_id,
    queue_recv_block_queue_id == {q} && ({or_terms(no_wait_recv_terms, '     ')})
  )

def violation_no_wait_send_timed_out_q{q} :=
  filter(
    queue_send_timeout_task_id,
    queue_send_timeout_queue_id == {q} && ({or_terms(no_wait_send_timeout_terms, '     ')})
  )

def violation_no_wait_receive_timed_out_q{q} :=
  filter(
    queue_recv_timeout_task_id,
    queue_recv_timeout_queue_id == {q} && ({or_terms(no_wait_recv_timeout_terms, '     ')})
  )

def violation_wait_forever_send_timed_out_q{q} :=
  filter(
    queue_send_timeout_task_id,
    queue_send_timeout_queue_id == {q} && ({or_terms(forever_send_terms, '     ')})
  )

def violation_wait_forever_receive_timed_out_q{q} :=
  filter(
    queue_recv_timeout_task_id,
    queue_recv_timeout_queue_id == {q} && ({or_terms(forever_recv_terms, '     ')})
  )

"""


def emit_untracked_queue_check(queue_ids: list[int]) -> str:
    """Reject events whose queue ID is absent from the configured queue map."""
    tracked_terms = [f"queue_event_id == {queue_id}" for queue_id in queue_ids]

    return f"""def queue_event_id :=
  {merge_streams(QUEUE_ID_STREAMS)}

def queue_event_is_tracked :=
  {or_terms(tracked_terms)}

def violation_untracked_queue :=
  filter(queue_event_id, queue_event_is_tracked == false)

"""


def emit_aggregate_outputs(queue_ids: list[int], mode: str) -> str:
    """Aggregate per-queue violations and emit the public outputs."""
    parts = []
    for check in CHECKS:
        per_queue = [f"violation_{check}_q{queue_id}" for queue_id in queue_ids]
        aggregate = f"violation_{check}"
        parts.append(f"def {aggregate} :=\n  {merge_streams(per_queue)}\n\n")
        if mode == "checks":
            parts.append(
                emit_pass_fail_pair(
                    public_name=check,
                    trigger_expr=CHECK_TRIGGERS[check],
                )
            )

    if mode == "checks":
        for check in GLOBAL_CHECKS:
            parts.append(
                emit_pass_fail_pair(
                    public_name=check,
                    trigger_expr=CHECK_TRIGGERS[check],
                )
            )

    output_checks = CHECKS + GLOBAL_CHECKS

    if mode == "violations":
        parts.extend(f"out violation_{check}\n" for check in output_checks)
    elif mode == "checks":
        for check in output_checks:
            parts.append(f"out FAIL_{check}\n")
            parts.append(f"out PASS_{check}\n")
    else:
        raise ValueError(f"invalid mode: {mode}")
    return "".join(parts)


def validate_options(
    max_tasks: int,
    queue_capacities: Mapping[int, int],
    mode: str,
) -> dict[int, int]:
    """Validate generator arguments and return queues sorted by ID."""
    if max_tasks <= 0:
        raise ValueError("max_tasks must be greater than zero")
    if mode not in {"violations", "checks"}:
        raise ValueError(f"invalid mode: {mode}")
    capacities = dict(queue_capacities)
    if not capacities:
        raise ValueError("queue_capacities must contain at least one queue")
    for queue_id, capacity in capacities.items():
        if not isinstance(queue_id, int) or queue_id < 0:
            raise ValueError(f"queue id must be a non-negative integer: {queue_id!r}")
        if not isinstance(capacity, int) or capacity <= 0:
            raise ValueError(f"queue capacity must be positive: queue {queue_id}")
    return dict(sorted(capacities.items()))


def generate(
    max_tasks: int,
    queue_capacities: Mapping[int, int],
    mode: str = "violations",
) -> str:
    """Return a queue monitor for the configured task range and queue map."""
    capacities = validate_options(max_tasks, queue_capacities, mode)
    parts = [emit_header()]
    parts.append(emit_untracked_queue_check(list(capacities)))
    for queue_id, capacity in capacities.items():
        parts.append(emit_queue(queue_id, capacity, max_tasks))
    parts.append(emit_aggregate_outputs(list(capacities), mode))
    return "\n".join(parts)
