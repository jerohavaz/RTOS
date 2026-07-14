#!/usr/bin/env python3

import argparse


STATE_CREATED = 0
STATE_READY = 1
STATE_RUNNING = 2
STATE_BLOCKED = 3


def emit_header() -> str:
    return """in state_id: Events[Int]
in state_old: Events[Int]
in state_new: Events[Int]

in ready_id: Events[Int]
in ready_prio: Events[Int]

in running_id: Events[Int]
in running_prio: Events[Int]

in blocked_id: Events[Int]

in idle: Events[Bool]

in tick: Events[Int]

in isr_enter_id: Events[Int]
in isr_exit_mode: Events[Int]

"""


def or_terms(terms: list[str]) -> str:
    if not terms:
        return "false"
    return " ||\n  ".join(terms)


def emit_per_task(task_id: int) -> str:
    return f"""def task_state_{task_id}: Events[Int] =
  merge(filter(state_new, state_id == {task_id}), {STATE_CREATED})

def ready_prio_{task_id}: Events[Int] =
  merge(filter(ready_prio, ready_id == {task_id}), -1)

def running_prio_{task_id}: Events[Int] =
  merge(filter(running_prio, running_id == {task_id}), -1)

"""


def emit_valid_state_transition() -> str:
    return f"""def valid_state_transition :=
  if state_old == {STATE_CREATED} then state_new == {STATE_READY}
  else if state_old == {STATE_READY} then state_new == {STATE_RUNNING} || state_new == {STATE_BLOCKED}
  else if state_old == {STATE_RUNNING} then state_new == {STATE_READY} || state_new == {STATE_BLOCKED}
  else if state_old == {STATE_BLOCKED} then state_new == {STATE_READY}
  else false

def violation_invalid_state_transition :=
  filter(state_id, valid_state_transition == false)

"""


def emit_running_id_bounds(max_tasks: int) -> str:
    return f"""def running_id_out_of_range :=
  running_id < 0 || running_id >= {max_tasks}

def violation_running_id_out_of_range :=
  filter(running_id, running_id_out_of_range)

"""


def emit_event_state_consistency(max_tasks: int) -> str:
    ready_terms = []
    running_terms = []
    blocked_terms = []

    for task_id in range(max_tasks):
        ready_terms.append(
            f"(ready_id == {task_id} && "
            f"last(task_state_{task_id}, ready_id) != {STATE_READY})"
        )

        running_terms.append(
            f"(running_id == {task_id} && "
            f"last(task_state_{task_id}, running_id) != {STATE_RUNNING})"
        )

        blocked_terms.append(
            f"(blocked_id == {task_id} && "
            f"last(task_state_{task_id}, blocked_id) != {STATE_BLOCKED})"
        )

    return f"""def ready_event_inconsistent :=
  {or_terms(ready_terms)}

def running_event_inconsistent :=
  {or_terms(running_terms)}

def blocked_event_inconsistent :=
  {or_terms(blocked_terms)}

def violation_ready_event_inconsistent :=
  filter(ready_id, ready_event_inconsistent)

def violation_running_event_inconsistent :=
  filter(running_id, running_event_inconsistent)

def violation_blocked_event_inconsistent :=
  filter(blocked_id, blocked_event_inconsistent)

"""


def emit_blocked_task_must_not_run(max_tasks: int) -> str:
    terms = []

    for task_id in range(max_tasks):
        terms.append(
            f"(running_id == {task_id} && "
            f"last(task_state_{task_id}, running_id) == {STATE_BLOCKED})"
        )

    return f"""def blocked_task_running :=
  {or_terms(terms)}

def violation_blocked_running :=
  filter(running_id, blocked_task_running)

"""


def emit_idle_check(max_tasks: int) -> str:
    ready_terms = []

    for task_id in range(max_tasks):
        ready_terms.append(
            f"(last(task_state_{task_id}, idle) == {STATE_READY})"
        )

    return f"""def any_task_ready_at_idle :=
  {or_terms(ready_terms)}

def idle_bad :=
  idle && any_task_ready_at_idle

def violation_idle_while_ready :=
  filter(idle, idle_bad)

"""


def emit_priority_check(max_tasks: int) -> str:
    terms = []

    for running_task in range(max_tasks):
        for ready_task in range(max_tasks):
            if running_task == ready_task:
                continue

            # Project convention:
            # higher numeric value = higher priority
            terms.append(
                f"(running_id == {running_task} && "
                f"last(task_state_{ready_task}, running_id) == {STATE_READY} && "
                f"last(ready_prio_{ready_task}, running_id) > running_prio)"
            )

    return f"""def higher_priority_task_ready :=
  {or_terms(terms)}

def violation_priority :=
  filter(running_id, higher_priority_task_ready)

"""


def emit_single_state_check() -> str:
    return """def multiple_states_bad :=
  false

def ready_and_blocked_bad :=
  false

def violation_multiple_states :=
  filter(state_id, multiple_states_bad)

def violation_ready_and_blocked :=
  filter(state_id, ready_and_blocked_bad)

"""


def emit_current_running() -> str:
    return """def current_running: Events[Int] =
  merge(running_id, -1)

def current_running_prio: Events[Int] =
  merge(running_prio, -1)

"""


def emit_quantum_and_round_robin(max_tasks: int, quantum_ticks: int) -> str:
    same_prio_ready_terms = []

    for current_task in range(max_tasks):
        for other_task in range(max_tasks):
            if current_task == other_task:
                continue

            same_prio_ready_terms.append(
                f"(last(current_running, tick) == {current_task} && "
                f"last(task_state_{other_task}, tick) == {STATE_READY} && "
                f"last(ready_prio_{other_task}, tick) == last(current_running_prio, tick))"
            )

    return f"""def running_ticks: Events[Int] =
  merge(
    if running_id >= 0 then 0
    else if idle then 0
    else if tick > 0 then last(running_ticks, tick) + tick
    else 0,
    0
  )

def same_priority_task_ready_at_tick :=
  {or_terms(same_prio_ready_terms)}

def quantum_due :=
  tick > 0 &&
  last(current_running, tick) >= 0 &&
  last(running_ticks, tick) + tick >= {quantum_ticks} &&
  same_priority_task_ready_at_tick

def rr_bad :=
  running_id >= 0 &&
  last(quantum_due, running_id) &&
  running_id == last(current_running, running_id)

def violation_quantum :=
  filter(running_id, rr_bad)

def violation_round_robin :=
  filter(running_id, rr_bad)

"""


def emit_isr_resume_check() -> str:
    return """def isr_seen_enter: Events[Bool] =
  merge(
    if isr_enter_id >= 0 then true
    else false,
    false
  )

def isr_exit_without_enter :=
  isr_exit_mode >= 0 &&
  last(isr_seen_enter, isr_exit_mode) == false

def violation_isr_resume :=
  filter(isr_exit_mode, isr_exit_without_enter)

"""


def emit_outputs() -> str:
    return """out violation_running_id_out_of_range
out violation_invalid_state_transition
out violation_ready_event_inconsistent
out violation_running_event_inconsistent
out violation_blocked_event_inconsistent
out violation_blocked_running
out violation_idle_while_ready
out violation_priority
out violation_multiple_states
out violation_ready_and_blocked
out violation_quantum
out violation_round_robin
out violation_isr_resume
"""


def generate(max_tasks: int, quantum_ticks: int) -> str:
    parts = []

    parts.append(emit_header())

    for task_id in range(max_tasks):
        parts.append(emit_per_task(task_id))

    parts.append(emit_valid_state_transition())
    parts.append(emit_running_id_bounds(max_tasks))
    parts.append(emit_event_state_consistency(max_tasks))
    parts.append(emit_blocked_task_must_not_run(max_tasks))
    parts.append(emit_idle_check(max_tasks))
    parts.append(emit_priority_check(max_tasks))
    parts.append(emit_single_state_check())
    parts.append(emit_current_running())
    parts.append(emit_quantum_and_round_robin(max_tasks, quantum_ticks))
    parts.append(emit_isr_resume_check())
    parts.append(emit_outputs())

    return "\n".join(parts)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate minimal bounded TeSSLa scheduler/task monitor."
    )

    parser.add_argument("--max-tasks", type=int, required=True)
    parser.add_argument("--quantum", type=int, required=True)
    parser.add_argument("-o", "--output", required=True)

    args = parser.parse_args()

    if args.max_tasks <= 0:
        raise SystemExit("max-tasks must be > 0")

    if args.quantum <= 0:
        raise SystemExit("quantum must be > 0")

    spec = generate(args.max_tasks, args.quantum)

    with open(args.output, "w", encoding="utf-8") as file:
        file.write(spec)


if __name__ == "__main__":
    main()