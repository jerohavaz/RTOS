#!/usr/bin/env python3

import argparse
import socket
import sys
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
    "ISR_ENTER": [
        ("isr_enter_id", int),
    ],
    "ISR_EXIT": [
        ("isr_exit_mode", int),
    ],
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert SEGGER RTT scheduler trace lines to TeSSLa input."
    )

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
        help="Read from stdin instead of RTT socket.",
    )

    parser.add_argument(
        "-o",
        "--output",
        default=None,
        help="Output file. Default: stdout.",
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

    expected_len = 1 + len(mapping)

    if len(parts) != expected_len:
        return []

    output_lines: list[str] = []

    for index, (stream_name, converter) in enumerate(mapping):
        raw_value = parts[index + 1]

        try:
            value = converter(raw_value)
        except ValueError:
            return []

        output_lines.append(
            f"{timestamp}: {stream_name} = {format_value(value)}"
        )

    return output_lines


def read_from_socket(host: str, port: int) -> Iterable[str]:
    with socket.create_connection((host, port)) as sock:
        with sock.makefile("r", encoding="utf-8", errors="ignore") as stream:
            for line in stream:
                yield line


def read_from_stdin() -> Iterable[str]:
    for line in sys.stdin:
        yield line


def open_output(path: str | None) -> TextIO:
    if path is None:
        return sys.stdout

    return open(path, "w", encoding="utf-8")


def main() -> int:
    args = parse_args()

    source = read_from_stdin() if args.stdin else read_from_socket(
        args.host,
        args.port,
    )

    timestamp = 0

    with open_output(args.output) as out:
        for raw_line in source:
            line = clean_line(raw_line)

            if line is None:
                continue

            converted_lines = convert_line(line, timestamp)

            if not converted_lines:
                continue

            for converted in converted_lines:
                out.write(converted)
                out.write("\n")

            out.flush()
            timestamp += 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())