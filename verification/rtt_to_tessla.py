import argparse
import socket
import sys
from contextlib import nullcontext
from typing import Callable, Iterable, TextIO

EVENTS: dict[str, list[tuple[str, Callable[[str], object]]]] = {
    "TASK_CREATE": [
        ("task_create_id", int),
        ("task_create_prio", int),
    ],
    "STATE": [
        ("state_id", int),
        ("state_old", int),
        ("state_new", int),
    ],
    "READY": [
        ("ready_id", int),
        ("ready_prio", int),
    ],
    "RUNNING": [
        ("running_id", int),
        ("running_prio", int),
    ],
    "BLOCKED": [
        ("blocked_id", int),
    ],
    "IDLE": [
        ("idle", lambda _value: True),
    ],
    "TICK": [
        ("tick", int),
    ],
    "DELAY_BUSY_START": [
        ("delay_busy_start_id", int),
        ("delay_busy_start_ticks", int),
    ],
    "DELAY_BUSY_END": [
        ("delay_busy_end_id", int),
    ],
    "DELAY_START": [
        ("delay_start_id", int),
        ("delay_start_ticks", int),
    ],
    "DELAY_END": [
        ("delay_end_id", int),
    ],
    "SEM_CREATE": [
        ("sem_create_id", int),
        ("sem_create_initial_count", int),
        ("sem_create_max_count", int),
    ],
    "SEM_ACQUIRE_ENTER": [
        ("sem_acquire_enter_id", int),
        ("sem_acquire_enter_task", int),
        ("sem_acquire_enter_count", int),
        ("sem_acquire_enter_timeout", int),
        ("sem_acquire_enter_finite", int),
    ],
    "SEM_ACQUIRE_EXIT": [
        ("sem_acquire_exit_id", int),
        ("sem_acquire_exit_task", int),
        ("sem_acquire_exit_count", int),
        ("sem_acquire_exit_succeeded", int),
    ],
    "SEM_BLOCK": [
        ("sem_block_id", int),
        ("sem_block_task", int),
        ("sem_block_prio", int),
        ("sem_block_timeout", int),
        ("sem_block_finite", int),
    ],
    "SEM_TIMEOUT": [
        ("sem_timeout_id", int),
        ("sem_timeout_task", int),
        ("sem_timeout_count", int),
    ],
    "SEM_RELEASE": [
        ("sem_release_id", int),
        ("sem_release_count_before", int),
        ("sem_release_count_after", int),
        ("sem_release_max_count", int),
        ("sem_release_succeeded", int),
    ],
    "SEM_WAKE": [
        ("sem_wake_id", int),
        ("sem_wake_task", int),
        ("sem_wake_prio", int),
    ],
    "MUTEX_CREATE": [
        ("mutex_create_id", int),
    ],
    "MUTEX_LOCK_ENTER": [
        ("mutex_lock_enter_id", int),
        ("mutex_lock_enter_task", int),
        ("mutex_lock_enter_owner", int),
        ("mutex_lock_enter_timeout", int),
        ("mutex_lock_enter_finite", int),
    ],
    "MUTEX_LOCK_EXIT": [
        ("mutex_lock_exit_id", int),
        ("mutex_lock_exit_task", int),
        ("mutex_lock_exit_owner", int),
        ("mutex_lock_exit_succeeded", int),
    ],
    "MUTEX_BLOCK": [
        ("mutex_block_id", int),
        ("mutex_block_task", int),
        ("mutex_block_prio", int),
        ("mutex_block_owner", int),
        ("mutex_block_timeout", int),
        ("mutex_block_finite", int),
    ],
    "MUTEX_TIMEOUT": [
        ("mutex_timeout_id", int),
        ("mutex_timeout_task", int),
        ("mutex_timeout_owner", int),
    ],
    "MUTEX_UNLOCK": [
        ("mutex_unlock_id", int),
        ("mutex_unlock_task", int),
        ("mutex_unlock_owner_before", int),
        ("mutex_unlock_owner_after", int),
        ("mutex_unlock_succeeded", int),
    ],
    "MUTEX_WAKE": [
        ("mutex_wake_id", int),
        ("mutex_wake_task", int),
        ("mutex_wake_prio", int),
    ],
    "QUEUE_CREATE": [
        ("queue_create_id", int),
        ("queue_create_capacity", int),
    ],
    "QUEUE_SEND_ATTEMPT": [
        ("queue_send_attempt_queue_id", int),
        ("queue_send_attempt_task_id", int),
        ("queue_send_attempt_task_prio", int),
        ("queue_send_attempt_timeout", int),
        ("queue_send_attempt_hash", int),
    ],
    "QUEUE_SEND_SUCCESS": [
        ("queue_send_success_queue_id", int),
        ("queue_send_success_task_id", int),
        ("queue_send_success_hash", int),
    ],
    "QUEUE_SEND_BLOCK": [
        ("queue_send_block_queue_id", int),
        ("queue_send_block_task_id", int),
        ("queue_send_block_task_prio", int),
    ],
    "QUEUE_SEND_TIMEOUT": [
        ("queue_send_timeout_queue_id", int),
        ("queue_send_timeout_task_id", int),
    ],
    "QUEUE_RECV_ATTEMPT": [
        ("queue_recv_attempt_queue_id", int),
        ("queue_recv_attempt_task_id", int),
        ("queue_recv_attempt_task_prio", int),
        ("queue_recv_attempt_timeout", int),
    ],
    "QUEUE_RECV_SUCCESS": [
        ("queue_recv_success_queue_id", int),
        ("queue_recv_success_task_id", int),
        ("queue_recv_success_hash", int),
    ],
    "QUEUE_RECV_BLOCK": [
        ("queue_recv_block_queue_id", int),
        ("queue_recv_block_task_id", int),
        ("queue_recv_block_task_prio", int),
    ],
    "QUEUE_RECV_TIMEOUT": [
        ("queue_recv_timeout_queue_id", int),
        ("queue_recv_timeout_task_id", int),
    ],
    "QUEUE_WAKE_SEND": [
        ("queue_wake_send_queue_id", int),
        ("queue_wake_send_task_id", int),
    ],
    "QUEUE_WAKE_RECV": [
        ("queue_wake_recv_queue_id", int),
        ("queue_wake_recv_task_id", int),
    ],
    "QUEUE_HANDOFF": [
        ("queue_handoff_queue_id", int),
        ("queue_handoff_sender_id", int),
        ("queue_handoff_receiver_id", int),
        ("queue_handoff_hash", int),
    ],
    "QUEUE_FILL": [
        ("queue_fill_queue_id", int),
        ("queue_fill_value", int),
    ],
}

last_trace_sequence: int | None = None
missing_trace_records = 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert SEGGER RTT trace lines to TeSSLa input.")

    parser.add_argument(
        "--host",
        default="127.0.0.1",
        help="RTT server host. Default: 127.0.0.1.",
    )

    parser.add_argument(
        "--port",
        type=int,
        default=19021,
        help="RTT server port. Default: 19021.",
    )

    parser.add_argument(
        "--channel",
        type=int,
        default=0,
        help=("RTT up-buffer channel containing the TeSSLa text trace. " "Default: 0."),
    )

    parser.add_argument(
        "--stdin",
        action="store_true",
        help="Read trace lines from stdin instead of the RTT socket.",
    )

    output_group = parser.add_mutually_exclusive_group()

    output_group.add_argument(
        "-o",
        "--output",
        metavar="FILE",
        help="Write converted TeSSLa input to FILE.",
    )

    output_group.add_argument(
        "--stdout",
        action="store_true",
        help="Write converted TeSSLa input to stdout.",
    )

    parser.add_argument(
        "--no-summary",
        action="store_false",
        dest="summary",
        default=True,
        help="Disable the live received and dropped event totals.",
    )

    return parser.parse_args()


def clean_line(line: str) -> str | None:
    line = line.strip()

    if not line:
        return None

    garbage_prefixes = (
        "###RTT Client:",
        "SEGGER ",
        "Process:",
        "Connecting",
        "Connected",
        "Searching",
        "Found",
        "Reading",
        "Channel",
    )

    if line.startswith(garbage_prefixes):
        return None

    if line == "TESSLA_START":
        return None

    parts = line.split()

    if not parts:
        return None

    event = parts[0]

    if event not in EVENTS:
        return None

    return line


def parse_trace_record(line: str) -> tuple[str | None, int]:
    """Remove trace metadata and report missing logical records.

    Legacy, unsequenced input remains supported for existing fixtures and
    manually authored traces.
    """
    global last_trace_sequence
    global missing_trace_records

    stripped = line.strip()

    if stripped == "TESSLA_START":
        last_trace_sequence = None
        return None, 0

    if not stripped.startswith("TRACE "):
        return stripped, 0

    parts = stripped.split(maxsplit=2)

    if len(parts) != 3:
        print(f"Malformed trace record: {stripped}", file=sys.stderr)
        return None, 0

    try:
        sequence = int(parts[1])
    except ValueError:
        print(f"Invalid trace sequence: {parts[1]}", file=sys.stderr)
        return None, 0

    missing = 0

    if last_trace_sequence is not None:
        distance = (sequence - last_trace_sequence) & 0xFFFFFFFF

        if distance != 1:
            missing = distance - 1
            missing_trace_records += missing
            expected = (last_trace_sequence + 1) & 0xFFFFFFFF

            print(
                f"Trace incomplete: expected sequence {expected}, " f"received {sequence}; {missing} record(s) missing",
                file=sys.stderr,
            )

    last_trace_sequence = sequence
    return parts[2], missing


def format_value(value: object) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"

    return str(value)


def convert_line(line: str, timestamp: int) -> list[str]:
    parts = line.split()
    event = parts[0]
    mapping = EVENTS[event]

    if event == "IDLE":
        if len(parts) != 1:
            return []

        return [f"{timestamp}: idle = true"]

    expected_length = 1 + len(mapping)

    if len(parts) != expected_length:
        return []

    output_lines: list[str] = []

    for index, (stream_name, converter) in enumerate(mapping):
        raw_value = parts[index + 1]

        try:
            value = converter(raw_value)
        except (TypeError, ValueError):
            return []

        output_lines.append(f"{timestamp}: {stream_name} = {format_value(value)}")

    return output_lines


def read_from_socket(
    host: str,
    port: int,
    channel: int,
) -> Iterable[str]:
    with socket.create_connection((host, port)) as sock:
        config = f"$$SEGGER_TELNET_ConfigStr=RTTCh;{channel}$$\n"
        sock.sendall(config.encode("ascii"))

        with sock.makefile(
            "r",
            encoding="utf-8",
            errors="ignore",
        ) as stream:
            yield from stream


def read_from_stdin() -> Iterable[str]:
    yield from sys.stdin


def open_output(path: str | None) -> TextIO:
    if path is None:
        return sys.stdout

    return open(path, "w", encoding="utf-8")


def print_live_summary(received_events: int) -> None:
    print(
        f"\rTrace summary: received={received_events}, " f"dropped={missing_trace_records}",
        end="",
        flush=True,
        file=sys.stderr,
    )


def main() -> int:
    args = parse_args()

    if args.channel < 0:
        print("RTT channel must be non-negative.", file=sys.stderr)
        return 1

    if args.stdin:
        source = read_from_stdin()
    else:
        source = read_from_socket(args.host, args.port, args.channel)

    timestamp = 0
    received_events = 0
    show_live_summary = args.output is not None and args.summary

    output_context = (
        nullcontext(sys.stdout)
        if args.stdout or args.output is None
        else open(
            args.output,
            "w",
            encoding="utf-8",
        )
    )

    try:
        with output_context as output_stream:
            for raw_line in source:
                event_line, missing = parse_trace_record(raw_line)

                if event_line is None:
                    continue

                wrote_integrity_event = False

                if missing > 0:
                    output_stream.write(f"{timestamp}: trace_incomplete = {missing}\n")
                    wrote_integrity_event = True

                line = clean_line(event_line)

                if line is None:
                    if wrote_integrity_event:
                        output_stream.flush()
                        timestamp += 1

                    if show_live_summary and missing > 0:
                        print_live_summary(received_events)

                    continue

                converted_lines = convert_line(
                    line,
                    timestamp,
                )

                if not converted_lines:
                    if wrote_integrity_event:
                        output_stream.flush()
                        timestamp += 1

                    if show_live_summary and missing > 0:
                        print_live_summary(received_events)

                    continue

                for converted_line in converted_lines:
                    output_stream.write(converted_line)
                    output_stream.write("\n")

                output_stream.flush()
                timestamp += 1
                received_events += 1

                if show_live_summary:
                    print_live_summary(received_events)

        if show_live_summary:
            # Finish the carriage-return-based live summary line.
            print(file=sys.stderr)

            if missing_trace_records > 0:
                print(
                    "Trace incomplete: TeSSLa results are inconclusive.",
                    file=sys.stderr,
                )

    except ConnectionRefusedError:
        if show_live_summary:
            print(file=sys.stderr)

        print(
            f"Could not connect to RTT server at " f"{args.host}:{args.port}.",
            file=sys.stderr,
        )
        return 1

    except OSError as error:
        if show_live_summary:
            print(file=sys.stderr)

        print(f"I/O error: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
