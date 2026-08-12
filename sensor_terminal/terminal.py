import argparse
import threading
import time
from collections import deque

import serial
from prompt_toolkit import PromptSession
from prompt_toolkit.completion import NestedCompleter
from prompt_toolkit.patch_stdout import patch_stdout


latest_sample = None
first_sample_time = None

# 11 timestamps = 10 time intervals between messages.
recent_sample_times = deque(maxlen=11)

latest_lock = threading.Lock()
stop_event = threading.Event()


def extract_message(line):
    """Remove the firmware CLI prompt from asynchronous messages."""
    markers = ("DATA,", "RESP,", "ERROR,", "TYPE,")

    for marker in markers:
        position = line.find(marker)
        if position >= 0:
            return line[position:]

    return line.replace("CLI> ", "").strip()


def serial_reader(ser, session):
    """Continuously receive and process messages from the STM32."""
    global latest_sample
    global first_sample_time

    while not stop_event.is_set():
        try:
            raw = ser.readline()

            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").strip()

            if not line:
                continue

            line = extract_message(line)

            if line.startswith("DATA,"):
                fields = line.split(",")

                if len(fields) != 8:
                    print(f"[Malformed data] {line}")
                    continue

                received_at = time.monotonic()

                try:
                    sample = {
                        "ax": float(fields[1]),
                        "ay": float(fields[2]),
                        "az": float(fields[3]),
                        "gx": float(fields[4]),
                        "gy": float(fields[5]),
                        "gz": float(fields[6]),
                        "count": int(fields[7]),
                        "time": received_at,
                    }

                except ValueError:
                    print(f"[Invalid data] {line}")
                    continue

                with latest_lock:
                    if first_sample_time is None:
                        first_sample_time = received_at

                    recent_sample_times.append(received_at)
                    latest_sample = sample

                try:
                    session.app.invalidate()
                except Exception:
                    pass

            elif line.startswith("RESP,"):
                print(f"[Response] {line[5:]}")

            elif line.startswith("ERROR,"):
                print(f"[Error] {line[6:]}")

            elif not line.startswith("TYPE,"):
                print(line)

        except (serial.SerialException, OSError, TypeError) as error:
            if not stop_event.is_set():
                print(f"[Serial error] {error}")

            break


def bottom_toolbar():
    """Display the latest sample and receive timing statistics."""

    with latest_lock:
        sample = latest_sample
        first_time = first_sample_time
        sample_times = tuple(recent_sample_times)

    if sample is None:
        return " Waiting for sensor data... "

    now = time.monotonic()

    # Time since the latest message arrived.
    age_ms = (now - sample["time"]) * 1000.0

    # Time since the first DATA message arrived.
    total_age_ms = (now - first_time) * 1000.0

    # Calculate message-to-message intervals in milliseconds.
    intervals_ms = [
        (newer - older) * 1000.0
        for older, newer in zip(sample_times, sample_times[1:])
    ]

    if intervals_ms:
        average_interval_ms = sum(intervals_ms) / len(intervals_ms)
        error_ms = average_interval_ms - 100.0
    else:
        average_interval_ms = 0.0
        error_ms = 0.0

    return (
        f" ACC: "
        f"{sample['ax']:+.3f} "
        f"{sample['ay']:+.3f} "
        f"{sample['az']:+.3f} g"
        f" | GYRO: "
        f"{sample['gx']:+.3f} "
        f"{sample['gy']:+.3f} "
        f"{sample['gz']:+.3f} deg/s"
        f" | samples={sample['count']:3d}"
        f" | age={age_ms:6.0f} ms"
        f" | avg({len(intervals_ms):2d})={average_interval_ms:6.1f} ms"
        f" | error={error_ms:+6.1f} ms"
        f" | total={total_age_ms:10.0f} ms "
    )


def send_command(ser, command):
    """Send one newline-terminated command to the STM32."""
    command = command.strip()

    if not command:
        return

    ser.write((command + "\r\n").encode("utf-8"))
    ser.flush()


def main():
    parser = argparse.ArgumentParser(
        description="Interactive STM32 LSM6DSL sensor terminal"
    )

    parser.add_argument(
        "port",
        help="Serial port, for example COM5 or /dev/ttyACM0",
    )

    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="UART baud rate (default: 115200)",
    )

    args = parser.parse_args()

    completer = NestedCompleter.from_nested_dict(
        {
            "help": None,
            "status": None,
            "reset": None,
            "mode": {
                "low": None,
                "normal": None,
                "high": None,
            },
            "stream": {
                "on": None,
                "off": None,
            },
            "led": {
                "on": None,
                "off": None,
            },
            "clear": None,
            "quit": None,
            "exit": None,
        }
    )

    session = PromptSession(
        completer=completer,
        complete_while_typing=True,
    )

    try:
        ser = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.2,
        )

    except serial.SerialException as error:
        print(f"Cannot open {args.port}: {error}")
        return

    stop_event.clear()

    reader = threading.Thread(
        target=serial_reader,
        args=(ser, session),
        name="serial_reader",
        daemon=True,
    )

    reader.start()

    print(f"Connected to {args.port} at {args.baud} baud.")
    print("Use Tab for completion. Enter 'quit' to exit.")

    try:
        with patch_stdout(raw=True):
            while True:
                command = session.prompt(
                    "cmd> ",
                    bottom_toolbar=bottom_toolbar,
                    refresh_interval=0.02,
                ).strip()

                if not command:
                    continue

                if command in ("quit", "exit"):
                    break

                if command == "clear":
                    print(
                        "\033[2J\033[H",
                        end="",
                        flush=True,
                    )
                    continue

                try:
                    send_command(ser, command)

                except serial.SerialException as error:
                    print(f"[Serial error] {error}")
                    break

    except (KeyboardInterrupt, EOFError):
        pass

    finally:
        stop_event.set()

        try:
            ser.cancel_read()

        except (
            AttributeError,
            OSError,
            serial.SerialException,
        ):
            pass

        reader.join(timeout=1.0)

        if ser.is_open:
            ser.close()

        print("Disconnected.")


if __name__ == "__main__":
    main()