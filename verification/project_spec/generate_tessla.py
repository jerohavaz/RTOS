"""Generate the project transmission-period TeSSLa verification monitor.

Author: Martin
Author: Jerome
"""

CHECKS = [
    ("transmission_interval", "transmission_interval"),
]


def emit_header(target_interval_ticks: int, jitter_ticks: int) -> str:
    """Emit monitor documentation and input declarations."""
    minimum = target_interval_ticks - jitter_ticks
    maximum = target_interval_ticks + jitter_ticks

    return f"""# Module: project
# Purpose: Verify transmission-complete timing against a target period with jitter.
# Generator: project_spec/generate_tessla.py
# Target interval: {target_interval_ticks} ticks
# Jitter/tolerance: +/- {jitter_ticks} ticks
# Accepted interval: [{minimum}, {maximum}] ticks (inclusive)

in tick: Events[Int]
in transmission_complete: Events[Bool]

"""


def emit_transmission_check(target_interval_ticks: int, jitter_ticks: int) -> str:
    """Emit the tick accumulator and transmission interval check."""
    minimum = target_interval_ticks - jitter_ticks
    maximum = target_interval_ticks + jitter_ticks

    return f"""# Accumulate positive OS tick values. This follows the same tick model used
# by the delay, semaphore, mutex, and queue verification modules.
def project_tick_sum: Events[Int] =
  merge(
    if tick > 0 then default(last(project_tick_sum, tick), 0) + tick else default(last(project_tick_sum, tick), 0),
    0
  )

# Snapshot the accumulated tick count immediately before each completion event.
def project_completion_tick :=
  default(last(project_tick_sum, transmission_complete), 0)

# The previous completion snapshot is -1 when no transmission has completed yet.
def project_previous_completion_tick :=
  default(last(project_completion_tick, transmission_complete), -1)

# For the first completion, measure from tick count zero. Afterwards, measure
# from the preceding transmission_complete event.
def project_interval_ticks :=
  if project_previous_completion_tick < 0
  then project_completion_tick
  else project_completion_tick - project_previous_completion_tick

# A transmission is valid at both jitter boundaries. For target=100 and
# tolerance=5, intervals 95 through 105 are accepted.
def project_interval_out_of_range :=
  project_interval_ticks < {minimum} || project_interval_ticks > {maximum}

def violation_transmission_interval :=
  filter(transmission_complete, project_interval_out_of_range)

# Check-mode marker: one result for every transmission_complete event.
def project_transmission_interval_check_marker :=
  if transmission_complete then 1 else 0

def FAIL_transmission_interval :=
  filter(project_transmission_interval_check_marker, project_interval_out_of_range)

def PASS_transmission_interval :=
  filter(project_transmission_interval_check_marker, project_interval_out_of_range == false)

"""


def emit_outputs(mode: str) -> str:
    """Emit public outputs for the requested monitor mode."""
    if mode == "violations":
        return "out violation_transmission_interval\n"

    if mode == "checks":
        return "out FAIL_transmission_interval\nout PASS_transmission_interval\n"

    raise ValueError(f"invalid mode: {mode}")


def generate(
    target_interval_ticks: int,
    jitter_ticks: int,
    mode: str = "violations",
) -> str:
    """Return the project monitor in violation or check output mode."""
    if target_interval_ticks <= 0:
        raise ValueError("target_interval_ticks must be greater than 0")

    if jitter_ticks < 0:
        raise ValueError("jitter_ticks must be non-negative")

    if jitter_ticks > target_interval_ticks:
        raise ValueError("jitter_ticks must not exceed target_interval_ticks")

    return "\n".join(
        [
            emit_header(target_interval_ticks, jitter_ticks),
            emit_transmission_check(target_interval_ticks, jitter_ticks),
            emit_outputs(mode),
        ]
    )
