from collections import deque
from dataclasses import dataclass, field
import time


COMMAND_TREE = {
    "help": (),
    "status": (),
    "reset": (),
    "mode": ("low", "normal", "high"),
    "stream": ("on", "off"),
    "led": ("on", "off"),
    "clear": (),
    "quit": (),
    "exit": (),
}


@dataclass
class Sample:
    ax: float
    ay: float
    az: float
    gx: float  # deg/s
    gy: float  # deg/s
    gz: float  # deg/s


@dataclass
class ProtocolStats:
    first_sample_time: float | None = None
    latest_sample_time: float | None = None
    latest_count: int | None = None
    recent_sample_times: deque = field(default_factory=lambda: deque(maxlen=11))

    def record_sample(self, received_at: float, count: int | None):
        if self.first_sample_time is None:
            self.first_sample_time = received_at
        self.latest_sample_time = received_at
        self.latest_count = count
        self.recent_sample_times.append(received_at)

    def timing_text(self):
        if self.latest_sample_time is None or self.first_sample_time is None:
            return "waiting for sensor data"

        now = time.monotonic()
        age_s = now - self.latest_sample_time
        total_s = now - self.first_sample_time
        times = tuple(self.recent_sample_times)
        intervals_ms = [
            (newer - older) * 1000.0
            for older, newer in zip(times, times[1:])
        ]

        if intervals_ms:
            avg_ms = sum(intervals_ms) / len(intervals_ms)
            error_ms = avg_ms - 100.0
        else:
            avg_ms = 0.0
            error_ms = 0.0

        count_text = "?" if self.latest_count is None else str(self.latest_count)
        return (
            f"samples={count_text}  age={age_s:.1f}s  "
            f"avg({len(intervals_ms)})={avg_ms:.1f}ms  "
            f"error={error_ms:+.1f}ms  total={total_s:.1f}s"
        )


class SensorProtocol:
    """Parser for sensor_viewer UART messages."""

    @staticmethod
    def parse(line: str):
        line = line.strip()
        if not line:
            return "empty", None

        if line.startswith("DATA,"):
            fields = line.split(",")
            if len(fields) != 8:
                return "malformed", line

            try:
                sample = Sample(
                    ax=float(fields[1]),
                    ay=float(fields[2]),
                    az=float(fields[3]),
                    gx=float(fields[4]),
                    gy=float(fields[5]),
                    gz=float(fields[6]),
                )
                count = int(fields[7])
            except ValueError:
                return "invalid", line

            return "data", (sample, count)

        if line.startswith("RESP,"):
            return "response", line[5:]

        if line.startswith("ERROR,"):
            return "error", line[6:]

        if line.startswith("TYPE,"):
            return "type", line[5:]

        # Compatibility with the original visualizer's six-value CSV format.
        fields = [part.strip() for part in line.split(",")]
        if len(fields) == 6:
            try:
                values = [float(part) for part in fields]
            except ValueError:
                pass
            else:
                return "legacy_data", (Sample(*values), None)

        return "message", line


def validate_command(command: str):
    parts = command.strip().lower().split()
    if not parts:
        return False, ""

    name = parts[0]
    if name not in COMMAND_TREE:
        return False, f"Unknown command: {name}"

    options = COMMAND_TREE[name]
    if options:
        if len(parts) != 2 or parts[1] not in options:
            return False, f"Usage: {name} {'|'.join(options)}"
    elif len(parts) != 1:
        return False, f"Usage: {name}"

    return True, " ".join(parts)
