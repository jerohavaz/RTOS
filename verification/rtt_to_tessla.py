"""Convert binary SEGGER RTT records into TeSSLa input.

Records use little-endian ``<HBB`` headers: sequence, event ID, payload length.
Event ID 0 with sequence 0 and an empty payload starts a firmware session.

Author: Jerome
Author: Martin
"""

import argparse
import socket
import struct
import sys
from contextlib import nullcontext
from dataclasses import dataclass
from enum import IntEnum
from queue import SimpleQueue
from threading import Thread
from typing import Iterable, Iterator, TextIO

FLUSH_INTERVAL = 1000
SUMMARY_INTERVAL = 1000
HEADER = struct.Struct("<HBB")
SEQUENCE_MASK = 0xFFFF
SOCKET_READ_SIZE = 64 * 1024
SOCKET_RECEIVE_BUFFER_SIZE = 4 * 1024 * 1024


class EventId(IntEnum):
    SESSION_START = 0
    TASK_CREATE = 1
    STATE = 2
    READY = 3
    RUNNING = 4
    STOP_RUNNING = 5
    BLOCKED = 6
    IDLE = 7
    TICK = 8
    DELAY_BUSY_START = 9
    DELAY_BUSY_END = 10
    DELAY_START = 11
    DELAY_END = 12
    SEM_CREATE = 13
    SEM_ACQUIRE_ENTER = 14
    SEM_ACQUIRE_EXIT = 15
    SEM_BLOCK = 16
    SEM_TIMEOUT = 17
    SEM_RELEASE = 18
    SEM_WAKE = 19
    MUTEX_CREATE = 20
    MUTEX_LOCK_ENTER = 21
    MUTEX_LOCK_EXIT = 22
    MUTEX_BLOCK = 23
    MUTEX_TIMEOUT = 24
    MUTEX_UNLOCK = 25
    MUTEX_WAKE = 26
    QUEUE_CREATE = 27
    QUEUE_SEND_ATTEMPT = 28
    QUEUE_SEND_SUCCESS = 29
    QUEUE_SEND_BLOCK = 30
    QUEUE_SEND_TIMEOUT = 31
    QUEUE_RECV_ATTEMPT = 32
    QUEUE_RECV_SUCCESS = 33
    QUEUE_RECV_BLOCK = 34
    QUEUE_RECV_TIMEOUT = 35
    QUEUE_WAKE_SEND = 36
    QUEUE_WAKE_RECV = 37
    QUEUE_HANDOFF = 38
    QUEUE_FILL = 39
    TRANSMISSION_COMPLETE = 40
    LOG = 41


@dataclass(frozen=True)
class EventDefinition:
    payload: struct.Struct | None
    streams: tuple[str, ...] = ()
    pulse_stream: str | None = None


EVENTS: dict[EventId, EventDefinition] = {
    EventId.SESSION_START: EventDefinition(struct.Struct("<")),
    EventId.TASK_CREATE: EventDefinition(
        struct.Struct("<BB"), ("task_create_id", "task_create_prio")
    ),
    EventId.STATE: EventDefinition(
        struct.Struct("<BBB"), ("state_id", "state_old", "state_new")
    ),
    EventId.READY: EventDefinition(struct.Struct("<BB"), ("ready_id", "ready_prio")),
    EventId.RUNNING: EventDefinition(
        struct.Struct("<BB"), ("running_id", "running_prio")
    ),
    EventId.STOP_RUNNING: EventDefinition(struct.Struct("<")),
    EventId.BLOCKED: EventDefinition(struct.Struct("<B"), ("blocked_id",)),
    EventId.IDLE: EventDefinition(struct.Struct("<"), pulse_stream="idle"),
    EventId.TICK: EventDefinition(struct.Struct("<I"), ("tick",)),
    EventId.DELAY_BUSY_START: EventDefinition(
        struct.Struct("<BI"), ("delay_busy_start_id", "delay_busy_start_ticks")
    ),
    EventId.DELAY_BUSY_END: EventDefinition(
        struct.Struct("<B"), ("delay_busy_end_id",)
    ),
    EventId.DELAY_START: EventDefinition(
        struct.Struct("<BI"), ("delay_start_id", "delay_start_ticks")
    ),
    EventId.DELAY_END: EventDefinition(struct.Struct("<B"), ("delay_end_id",)),
    EventId.SEM_CREATE: EventDefinition(
        struct.Struct("<III"),
        ("sem_create_id", "sem_create_initial_count", "sem_create_max_count"),
    ),
    EventId.SEM_ACQUIRE_ENTER: EventDefinition(
        struct.Struct("<IBIIB"),
        (
            "sem_acquire_enter_id",
            "sem_acquire_enter_task",
            "sem_acquire_enter_count",
            "sem_acquire_enter_timeout",
            "sem_acquire_enter_finite",
        ),
    ),
    EventId.SEM_ACQUIRE_EXIT: EventDefinition(
        struct.Struct("<IBIB"),
        (
            "sem_acquire_exit_id",
            "sem_acquire_exit_task",
            "sem_acquire_exit_count",
            "sem_acquire_exit_succeeded",
        ),
    ),
    EventId.SEM_BLOCK: EventDefinition(
        struct.Struct("<IBBIB"),
        (
            "sem_block_id",
            "sem_block_task",
            "sem_block_prio",
            "sem_block_timeout",
            "sem_block_finite",
        ),
    ),
    EventId.SEM_TIMEOUT: EventDefinition(
        struct.Struct("<IBI"),
        ("sem_timeout_id", "sem_timeout_task", "sem_timeout_count"),
    ),
    EventId.SEM_RELEASE: EventDefinition(
        struct.Struct("<IIIIB"),
        (
            "sem_release_id",
            "sem_release_count_before",
            "sem_release_count_after",
            "sem_release_max_count",
            "sem_release_succeeded",
        ),
    ),
    EventId.SEM_WAKE: EventDefinition(
        struct.Struct("<IBB"), ("sem_wake_id", "sem_wake_task", "sem_wake_prio")
    ),
    EventId.MUTEX_CREATE: EventDefinition(struct.Struct("<I"), ("mutex_create_id",)),
    EventId.MUTEX_LOCK_ENTER: EventDefinition(
        struct.Struct("<IBBIB"),
        (
            "mutex_lock_enter_id",
            "mutex_lock_enter_task",
            "mutex_lock_enter_owner",
            "mutex_lock_enter_timeout",
            "mutex_lock_enter_finite",
        ),
    ),
    EventId.MUTEX_LOCK_EXIT: EventDefinition(
        struct.Struct("<IBBB"),
        (
            "mutex_lock_exit_id",
            "mutex_lock_exit_task",
            "mutex_lock_exit_owner",
            "mutex_lock_exit_succeeded",
        ),
    ),
    EventId.MUTEX_BLOCK: EventDefinition(
        struct.Struct("<IBBBIB"),
        (
            "mutex_block_id",
            "mutex_block_task",
            "mutex_block_prio",
            "mutex_block_owner",
            "mutex_block_timeout",
            "mutex_block_finite",
        ),
    ),
    EventId.MUTEX_TIMEOUT: EventDefinition(
        struct.Struct("<IBB"),
        ("mutex_timeout_id", "mutex_timeout_task", "mutex_timeout_owner"),
    ),
    EventId.MUTEX_UNLOCK: EventDefinition(
        struct.Struct("<IBBBB"),
        (
            "mutex_unlock_id",
            "mutex_unlock_task",
            "mutex_unlock_owner_before",
            "mutex_unlock_owner_after",
            "mutex_unlock_succeeded",
        ),
    ),
    EventId.MUTEX_WAKE: EventDefinition(
        struct.Struct("<IBB"), ("mutex_wake_id", "mutex_wake_task", "mutex_wake_prio")
    ),
    EventId.QUEUE_CREATE: EventDefinition(
        struct.Struct("<II"), ("queue_create_id", "queue_create_capacity")
    ),
    EventId.QUEUE_SEND_ATTEMPT: EventDefinition(
        struct.Struct("<IBBII"),
        (
            "queue_send_attempt_queue_id",
            "queue_send_attempt_task_id",
            "queue_send_attempt_task_prio",
            "queue_send_attempt_timeout",
            "queue_send_attempt_hash",
        ),
    ),
    EventId.QUEUE_SEND_SUCCESS: EventDefinition(
        struct.Struct("<IBI"),
        (
            "queue_send_success_queue_id",
            "queue_send_success_task_id",
            "queue_send_success_hash",
        ),
    ),
    EventId.QUEUE_SEND_BLOCK: EventDefinition(
        struct.Struct("<IBB"),
        (
            "queue_send_block_queue_id",
            "queue_send_block_task_id",
            "queue_send_block_task_prio",
        ),
    ),
    EventId.QUEUE_SEND_TIMEOUT: EventDefinition(
        struct.Struct("<IB"),
        ("queue_send_timeout_queue_id", "queue_send_timeout_task_id"),
    ),
    EventId.QUEUE_RECV_ATTEMPT: EventDefinition(
        struct.Struct("<IBBI"),
        (
            "queue_recv_attempt_queue_id",
            "queue_recv_attempt_task_id",
            "queue_recv_attempt_task_prio",
            "queue_recv_attempt_timeout",
        ),
    ),
    EventId.QUEUE_RECV_SUCCESS: EventDefinition(
        struct.Struct("<IBI"),
        (
            "queue_recv_success_queue_id",
            "queue_recv_success_task_id",
            "queue_recv_success_hash",
        ),
    ),
    EventId.QUEUE_RECV_BLOCK: EventDefinition(
        struct.Struct("<IBB"),
        (
            "queue_recv_block_queue_id",
            "queue_recv_block_task_id",
            "queue_recv_block_task_prio",
        ),
    ),
    EventId.QUEUE_RECV_TIMEOUT: EventDefinition(
        struct.Struct("<IB"),
        ("queue_recv_timeout_queue_id", "queue_recv_timeout_task_id"),
    ),
    EventId.QUEUE_WAKE_SEND: EventDefinition(
        struct.Struct("<IB"), ("queue_wake_send_queue_id", "queue_wake_send_task_id")
    ),
    EventId.QUEUE_WAKE_RECV: EventDefinition(
        struct.Struct("<IB"), ("queue_wake_recv_queue_id", "queue_wake_recv_task_id")
    ),
    EventId.QUEUE_HANDOFF: EventDefinition(
        struct.Struct("<IBBI"),
        (
            "queue_handoff_queue_id",
            "queue_handoff_sender_id",
            "queue_handoff_receiver_id",
            "queue_handoff_hash",
        ),
    ),
    EventId.QUEUE_FILL: EventDefinition(
        struct.Struct("<II"), ("queue_fill_queue_id", "queue_fill_value")
    ),
    EventId.TRANSMISSION_COMPLETE: EventDefinition(
        struct.Struct("<"), pulse_stream="transmission_complete"
    ),
    EventId.LOG: EventDefinition(None),
}

SESSION_START_RECORD = HEADER.pack(0, EventId.SESSION_START, 0)
last_trace_sequence: int | None = None
missing_trace_records = 0
summary_line_active = False


def reset_trace_state(sequence: int | None = None) -> None:
    global last_trace_sequence
    global missing_trace_records

    last_trace_sequence = sequence
    missing_trace_records = 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert binary SEGGER RTT trace records to TeSSLa input."
    )
    parser.add_argument("--host", default="127.0.0.1", help="RTT server host. Default: 127.0.0.1.")
    parser.add_argument("--port", type=int, default=19021, help="RTT server port. Default: 19021.")
    parser.add_argument(
        "--channel",
        type=int,
        default=0,
        help="RTT up-buffer channel containing the TeSSLa binary trace. Default: 0.",
    )
    parser.add_argument(
        "--stdin",
        action="store_true",
        help="Read binary trace records from stdin instead of the RTT socket.",
    )

    output_group = parser.add_mutually_exclusive_group()
    output_group.add_argument(
        "-o", "--output", metavar="FILE", help="Write converted TeSSLa input to FILE."
    )
    output_group.add_argument(
        "--stdout", action="store_true", help="Write converted TeSSLa input to stdout."
    )
    parser.add_argument(
        "--no-summary",
        action="store_false",
        dest="summary",
        default=True,
        help="Disable the live received and dropped event totals.",
    )
    return parser.parse_args()


def iter_binary_records(
    chunks: Iterable[bytes], *, sync_to_session: bool
) -> Iterator[tuple[int, int, bytes]]:
    """Reassemble records split across arbitrary socket or file chunks."""
    buffer = bytearray()
    synchronized = not sync_to_session

    for chunk in chunks:
        if not chunk:
            continue

        buffer.extend(chunk)

        while True:
            if not synchronized:
                marker_offset = buffer.find(SESSION_START_RECORD)

                if marker_offset < 0:
                    if len(buffer) > len(SESSION_START_RECORD) - 1:
                        del buffer[: -(len(SESSION_START_RECORD) - 1)]
                    break

                del buffer[:marker_offset]
                synchronized = True

            if len(buffer) < HEADER.size:
                break

            sequence, event_id, payload_length = HEADER.unpack_from(buffer)
            record_length = HEADER.size + payload_length

            if len(buffer) < record_length:
                break

            payload = bytes(buffer[HEADER.size:record_length])
            del buffer[:record_length]
            yield sequence, event_id, payload


def trace_gap(sequence: int) -> int:
    global last_trace_sequence
    global missing_trace_records
    global summary_line_active

    missing = 0

    if last_trace_sequence is not None:
        distance = (sequence - last_trace_sequence) & SEQUENCE_MASK

        if distance != 1:
            expected = (last_trace_sequence + 1) & SEQUENCE_MASK

            if distance > 1:
                missing = distance - 1
                missing_trace_records += missing

            if summary_line_active:
                print(file=sys.stderr)
                summary_line_active = False

            print(
                f"Trace incomplete: expected sequence {expected}, received {sequence}; "
                f"{missing} record(s) missing",
                file=sys.stderr,
            )

    last_trace_sequence = sequence
    return missing


def convert_record(event_id: int, payload: bytes, timestamp: int) -> list[str]:
    try:
        event = EventId(event_id)
    except ValueError:
        print(f"Unknown trace event ID: {event_id}", file=sys.stderr)
        return []

    definition = EVENTS[event]

    if definition.payload is None:
        return []

    if len(payload) != definition.payload.size:
        print(
            f"Malformed {event.name} payload: expected {definition.payload.size} byte(s), "
            f"received {len(payload)}",
            file=sys.stderr,
        )
        return []

    if definition.pulse_stream is not None:
        return [f"{timestamp}: {definition.pulse_stream} = true"]

    values = definition.payload.unpack(payload)
    return [
        f"{timestamp}: {stream_name} = {value}"
        for stream_name, value in zip(definition.streams, values, strict=True)
    ]


def read_from_socket(host: str, port: int, channel: int) -> Iterable[bytes]:
    with socket.create_connection((host, port)) as sock:
        sock.setsockopt(
            socket.SOL_SOCKET,
            socket.SO_RCVBUF,
            SOCKET_RECEIVE_BUFFER_SIZE,
        )
        config = f"$$SEGGER_TELNET_ConfigStr=RTTCh;{channel}$$\n"
        sock.sendall(config.encode("ascii"))
        print(
            f"Connected to RTT channel {channel} at {host}:{port}. "
            "Reset the STM32 now; waiting for the binary session-start record.",
            file=sys.stderr,
        )

        received: SimpleQueue[bytes | OSError | None] = SimpleQueue()

        def receive() -> None:
            try:
                while chunk := sock.recv(SOCKET_READ_SIZE):
                    received.put(chunk)
            except OSError as error:
                received.put(error)
            finally:
                received.put(None)

        Thread(target=receive, name="rtt-socket-reader", daemon=True).start()

        while True:
            item = received.get()

            if item is None:
                break

            if isinstance(item, OSError):
                raise item

            yield item


def read_from_stdin() -> Iterable[bytes]:
    while chunk := sys.stdin.buffer.read(4096):
        yield chunk


def print_live_summary(received_events: int) -> None:
    global summary_line_active

    print(
        f"\rTrace summary: received={received_events}, dropped={missing_trace_records}",
        end="",
        flush=True,
        file=sys.stderr,
    )
    summary_line_active = True


def start_session(
    output_stream: TextIO,
    *,
    output_path: str | None,
    replacing_session: bool,
    session_count: int,
    sequence: int,
) -> None:
    reset_trace_state(sequence)

    if output_path is not None:
        output_stream.seek(0)
        output_stream.truncate()
        output_stream.flush()
    elif replacing_session:
        print(
            "Target restarted: stdout consumers have already received the previous session "
            "and must also be restarted.",
            file=sys.stderr,
        )

    print(f"Trace session {session_count} started.", file=sys.stderr)


def main() -> int:
    args = parse_args()

    if args.channel < 0:
        print("RTT channel must be non-negative.", file=sys.stderr)
        return 1

    source = (
        read_from_stdin()
        if args.stdin
        else read_from_socket(args.host, args.port, args.channel)
    )
    records = iter_binary_records(source, sync_to_session=not args.stdin)
    timestamp = 0
    received_events = 0
    session_started = args.stdin
    session_count = 0
    show_live_summary = args.output is not None and args.summary
    reset_trace_state()

    output_context = (
        nullcontext(sys.stdout)
        if args.stdout or args.output is None
        else open(args.output, "w", encoding="utf-8")
    )

    try:
        with output_context as output_stream:
            for sequence, event_id, payload in records:
                if event_id == EventId.SESSION_START:
                    if sequence != 0 or payload:
                        print("Malformed session-start record.", file=sys.stderr)
                        continue

                    replacing_session = session_count > 0 or timestamp > 0
                    session_started = True
                    session_count += 1
                    timestamp = 0
                    received_events = 0
                    start_session(
                        output_stream,
                        output_path=args.output,
                        replacing_session=replacing_session,
                        session_count=session_count,
                        sequence=sequence,
                    )
                    continue

                if not session_started:
                    continue

                missing = trace_gap(sequence)
                wrote_integrity_event = False

                if missing > 0:
                    output_stream.write(f"{timestamp}: trace_incomplete = {missing}\n")
                    wrote_integrity_event = True

                converted_lines = convert_record(event_id, payload, timestamp)

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

                timestamp += 1
                received_events += 1

                if received_events % FLUSH_INTERVAL == 0:
                    output_stream.flush()

                if show_live_summary and received_events % SUMMARY_INTERVAL == 0:
                    print_live_summary(received_events)

        if show_live_summary:
            print(file=sys.stderr)

            if missing_trace_records > 0:
                print("Trace incomplete: TeSSLa results are inconclusive.", file=sys.stderr)

    except ConnectionRefusedError:
        if show_live_summary:
            print(file=sys.stderr)

        print(f"Could not connect to RTT server at {args.host}:{args.port}.", file=sys.stderr)
        return 1

    except OSError as error:
        if show_live_summary:
            print(file=sys.stderr)

        print(f"I/O error: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
