CHECKS = [
    ("trace_complete", "trace_incomplete"),
]


def emit_header() -> str:
    return """in trace_incomplete: Events[Int]

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


def emit_integrity_checks() -> str:
    return f"""# Any positive value represents the number of records missing
# before the current trace event.
def violation_trace_incomplete :=
  filter(trace_incomplete, trace_incomplete > 0)

{emit_pass_fail_pair(
    "trace_complete",
    "trace_incomplete",
    "trace_incomplete >= 0",
    "trace_incomplete > 0",
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


def generate(mode: str = "violations") -> str:
    parts = [
        emit_header(),
        emit_integrity_checks(),
        emit_outputs(mode),
    ]

    return "\n".join(parts)
