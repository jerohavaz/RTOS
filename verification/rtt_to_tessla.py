#!/usr/bin/env python3

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
    ],
    "DELAY_BUSY_END": [
        ("delay_busy_end_id", int),
    ],
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert SEGGER RTT scheduler trace lines to TeSSLa input.")

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


def read_from_socket(host: str, port: int) -> Iterable[str]:
    with socket.create_connection((host, port)) as sock:
        with sock.makefile("r", encoding="utf-8", errors="ignore") as stream:
            yield from stream


def read_from_stdin() -> Iterable[str]:
    yield from sys.stdin


def open_output(path: str | None) -> TextIO:
    if path is None:
        return sys.stdout

    return open(path, "w", encoding="utf-8")


def main() -> int:
    args = parse_args()

    if args.stdin:
        source = read_from_stdin()
    else:
        source = read_from_socket(args.host, args.port)

    timestamp = 0

    output_context = (
        nullcontext(sys.stdout) if args.stdout or args.output is None else open(args.output, "w", encoding="utf-8")
    )

    try:
        with output_context as output_stream:
            for raw_line in source:
                line = clean_line(raw_line)

                if line is None:
                    continue

                converted_lines = convert_line(line, timestamp)

                if not converted_lines:
                    continue

                for converted_line in converted_lines:
                    output_stream.write(converted_line)
                    output_stream.write("\n")

                output_stream.flush()
                timestamp += 1

    except ConnectionRefusedError:
        print(
            f"Could not connect to RTT server at " f"{args.host}:{args.port}.",
            file=sys.stderr,
        )
        return 1

    except OSError as error:
        print(f"I/O error: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
