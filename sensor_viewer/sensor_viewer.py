#!/usr/bin/env python3

import argparse
import math
import sys
import time
from collections import deque
from dataclasses import dataclass

import serial

try:
    from PySide6.QtCore import QPointF, QTimer, Qt
    from PySide6.QtGui import QBrush, QColor, QFontDatabase, QPainter, QPen, QPolygonF
    from PySide6.QtWidgets import (
        QApplication,
        QFrame,
        QGridLayout,
        QGroupBox,
        QHBoxLayout,
        QLabel,
        QMainWindow,
        QPushButton,
        QSizePolicy,
        QSplitter,
        QStatusBar,
        QTextEdit,
        QVBoxLayout,
        QWidget,
    )
except ModuleNotFoundError as error:
    if error.name and error.name.startswith("PySide6"):
        print(
            "PySide6 is required for the sensor_viewer GUI.\n"
            "Install the project dependencies with:\n"
            "  python3 -m pip install -r requirements.txt",
            file=sys.stderr,
        )
        sys.exit(2)
    raise

from sensor_protocol import ProtocolStats, Sample, SensorProtocol


APP_NAME = "sensor_viewer"
DEFAULT_SIZE = (1120, 720)
MIN_SIZE = (760, 520)


@dataclass
class Quaternion:
    w: float
    x: float
    y: float
    z: float

    def normalized(self):
        n = math.sqrt(self.w * self.w + self.x * self.x + self.y * self.y + self.z * self.z)
        if n < 1e-12:
            return Quaternion(1.0, 0.0, 0.0, 0.0)
        return Quaternion(self.w / n, self.x / n, self.y / n, self.z / n)

    def conjugate(self):
        return Quaternion(self.w, -self.x, -self.y, -self.z)

    def __mul__(self, other):
        return Quaternion(
            self.w * other.w - self.x * other.x - self.y * other.y - self.z * other.z,
            self.w * other.x + self.x * other.w + self.y * other.z - self.z * other.y,
            self.w * other.y - self.x * other.z + self.y * other.w + self.z * other.x,
            self.w * other.z + self.x * other.y - self.y * other.x + self.z * other.w,
        )

    def rotate_vector(self, v):
        qv = Quaternion(0.0, v[0], v[1], v[2])
        r = self * qv * self.conjugate()
        return r.x, r.y, r.z


def clamp(value, lo, hi):
    return max(lo, min(hi, value))


def vec_norm(v):
    return math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])


def vec_normalized(v):
    n = vec_norm(v)
    if n < 1e-12:
        return 0.0, 0.0, 1.0
    return v[0] / n, v[1] / n, v[2] / n


def vec_cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def vec_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def quat_from_axis_angle(axis, angle_rad):
    axis = vec_normalized(axis)
    half = 0.5 * angle_rad
    s = math.sin(half)
    return Quaternion(math.cos(half), axis[0] * s, axis[1] * s, axis[2] * s).normalized()


def quat_from_two_vectors(a, b):
    a = vec_normalized(a)
    b = vec_normalized(b)
    dot = clamp(vec_dot(a, b), -1.0, 1.0)

    if dot > 0.999999:
        return Quaternion(1.0, 0.0, 0.0, 0.0)

    if dot < -0.999999:
        axis = vec_cross((1.0, 0.0, 0.0), a)
        if vec_norm(axis) < 1e-6:
            axis = vec_cross((0.0, 1.0, 0.0), a)
        return quat_from_axis_angle(axis, math.pi)

    axis = vec_cross(a, b)
    return Quaternion(1.0 + dot, axis[0], axis[1], axis[2]).normalized()


class MahonyAttitudeEstimator:
    def __init__(self):
        self.q = Quaternion(1.0, 0.0, 0.0, 0.0)
        self.last_time = None
        self.kp = 2.0
        self.accel_tolerance_g = 0.22
        self.accel_lp = None
        self.accel_lp_alpha = 0.14

    def reset(self, sample: Sample):
        accel_body = (sample.ax, sample.ay, sample.az)
        self.q = quat_from_two_vectors(accel_body, (0.0, 0.0, 1.0))
        self.last_time = time.monotonic()
        self.accel_lp = None

    def update(self, sample: Sample):
        now = time.monotonic()
        if self.last_time is None:
            self.reset(sample)
            return self.q

        dt = now - self.last_time
        self.last_time = now
        if dt <= 0.0 or dt > 0.2:
            dt = 0.02

        wx = math.radians(sample.gx)
        wy = math.radians(sample.gy)
        wz = math.radians(sample.gz)
        wx, wy, wz = self.apply_mahony_accel_correction(sample, wx, wy, wz)
        self.integrate_corrected_gyro(wx, wy, wz, dt)
        self.q = self.q.normalized()
        return self.q

    def apply_mahony_accel_correction(self, sample: Sample, wx, wy, wz):
        accel_body_raw = (sample.ax, sample.ay, sample.az)
        accel_mag = vec_norm(accel_body_raw)
        if accel_mag < 1e-6:
            return wx, wy, wz

        mag_error = abs(accel_mag - 1.0)
        if mag_error > self.accel_tolerance_g:
            return wx, wy, wz

        confidence = 1.0 - mag_error / self.accel_tolerance_g
        confidence *= confidence
        accel_body = vec_normalized(accel_body_raw)

        if self.accel_lp is None:
            self.accel_lp = accel_body
        else:
            self.accel_lp = vec_normalized(
                (
                    self.accel_lp[0] + self.accel_lp_alpha * (accel_body[0] - self.accel_lp[0]),
                    self.accel_lp[1] + self.accel_lp_alpha * (accel_body[1] - self.accel_lp[1]),
                    self.accel_lp[2] + self.accel_lp_alpha * (accel_body[2] - self.accel_lp[2]),
                )
            )

        measured_up_world = vec_normalized(self.q.rotate_vector(self.accel_lp))
        error_world = vec_cross(measured_up_world, (0.0, 0.0, 1.0))
        error_body = self.q.conjugate().rotate_vector(error_world)
        gain = self.kp * confidence

        return (
            wx + gain * error_body[0],
            wy + gain * error_body[1],
            wz + gain * error_body[2],
        )

    def integrate_corrected_gyro(self, wx, wy, wz, dt):
        omega = math.sqrt(wx * wx + wy * wy + wz * wz)
        if omega < 1e-12:
            return

        angle = omega * dt
        axis_body = (wx / omega, wy / omega, wz / omega)
        dq = quat_from_axis_angle(axis_body, angle)
        self.q = (self.q * dq).normalized()


class VisualOffsetFilter:
    def __init__(self):
        self.x = 0.0
        self.y = 0.0
        self.z = 0.0
        self.response = 0.28
        self.max_offset = 1.15
        self.scale = 1.20
        self.deadband = 0.04

    def reset(self):
        self.x = self.y = self.z = 0.0

    def update(self, sample: Sample | None, q: Quaternion):
        if sample is None:
            target_x = target_y = target_z = 0.0
        else:
            measured_body = (sample.ax, sample.ay, sample.az)
            expected_body = q.conjugate().rotate_vector((0.0, 0.0, 1.0))
            lin_body = (
                measured_body[0] - expected_body[0],
                measured_body[1] - expected_body[1],
                measured_body[2] - expected_body[2],
            )
            lin_world = q.rotate_vector(lin_body)
            target_x = clamp(self.apply_deadband(lin_world[0]) * self.scale, -self.max_offset, self.max_offset)
            target_y = clamp(self.apply_deadband(lin_world[1]) * self.scale, -self.max_offset, self.max_offset)
            target_z = clamp(self.apply_deadband(lin_world[2]) * self.scale, -self.max_offset, self.max_offset)

        self.x += self.response * (target_x - self.x)
        self.y += self.response * (target_y - self.y)
        self.z += self.response * (target_z - self.z)
        return self.x, self.y, self.z

    def apply_deadband(self, value):
        if abs(value) < self.deadband:
            return 0.0
        return value - self.deadband if value > 0.0 else value + self.deadband


class UARTClient:
    def __init__(self, port, baud):
        self.ser = serial.Serial(
            port=port,
            baudrate=baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0,
        )
        self._rx_buffer = bytearray()

    def poll_lines(self, limit=64):
        waiting = self.ser.in_waiting
        if waiting:
            self._rx_buffer.extend(self.ser.read(waiting))

        lines = []
        while len(lines) < limit:
            newline_index = self._rx_buffer.find(b"\n")
            if newline_index < 0:
                break
            raw = bytes(self._rx_buffer[:newline_index])
            del self._rx_buffer[: newline_index + 1]
            lines.append(raw.rstrip(b"\r").decode("utf-8", errors="replace"))
        return lines

    def send_command(self, command):
        command = command.strip()
        if not command:
            return
        self.ser.write((command + "\r\n").encode("utf-8"))
        self.ser.flush()

    def close(self):
        if self.ser.is_open:
            self.ser.close()


class SensorCanvas(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.latest_sample = None
        self.q = Quaternion(1.0, 0.0, 0.0, 0.0)
        self.offset = (0.0, 0.0, 0.0)
        self.setMinimumSize(360, 320)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)

    def set_scene(self, sample, q, offset):
        self.latest_sample = sample
        self.q = q
        self.offset = offset
        self.update()

    @staticmethod
    def _rotate_x(v, angle_deg):
        a = math.radians(angle_deg)
        c, s = math.cos(a), math.sin(a)
        x, y, z = v
        return x, y * c - z * s, y * s + z * c

    @staticmethod
    def _rotate_z(v, angle_deg):
        a = math.radians(angle_deg)
        c, s = math.cos(a), math.sin(a)
        x, y, z = v
        return x * c - y * s, x * s + y * c, z

    def _camera_transform(self, point, apply_board=True):
        x, y, z = point
        if apply_board:
            x, y, z = self.q.rotate_vector((x, y, z))
            x += self.offset[0]
            y += self.offset[1]
            z += self.offset[2]

        x, y, z = self._rotate_z((x, y, z), -35.0)
        x, y, z = self._rotate_x((x, y, z), -55.0)
        return x + 0.18, y - 0.10, z - 6.0

    @staticmethod
    def _project(point, width, height):
        x, y, z = point
        if z >= -0.05:
            z = -0.05
        fov = math.radians(40.0)
        focal = (height * 0.5) / math.tan(fov * 0.5)
        scale = focal / (-z)
        return QPointF(width * 0.52 + x * scale, height * 0.57 - y * scale)

    def paintEvent(self, _event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
        painter.fillRect(self.rect(), QColor("#0b0d12"))
        width = max(1, self.width())
        height = max(1, self.height())
        self._draw_center_cross(painter, width, height)
        self._draw_board(painter, width, height)
        self._draw_hud(painter, width, height)
        self._draw_legend(painter, width, height)

    def _draw_center_cross(self, painter, width, height):
        origin = self._project(self._camera_transform((0.0, 0.0, 0.0), apply_board=False), width, height)
        painter.setPen(QPen(QColor("#7a7f89"), 1))
        for endpoint in ((0.35, 0.0, 0.0), (0.0, 0.35, 0.0), (0.0, 0.0, 0.35)):
            p2 = self._project(self._camera_transform(endpoint, apply_board=False), width, height)
            painter.drawLine(origin, p2)

    def _draw_board(self, painter, width, height):
        x, y, z = 1.4, 1.0, 0.12
        vertices = [
            (-x, -y, -z), (x, -y, -z), (x, y, -z), (-x, y, -z),
            (-x, -y, z), (x, -y, z), (x, y, z), (-x, y, z),
        ]
        transformed = [self._camera_transform(v, apply_board=True) for v in vertices]
        projected = [self._project(v, width, height) for v in transformed]

        faces = [
            ((4, 5, 6, 7), "#00cc20"),
            ((0, 3, 2, 1), "#262626"),
            ((7, 6, 2, 3), "#0040ff"),
            ((0, 1, 5, 4), "#ff7300"),
            ((5, 1, 2, 6), "#ff0d0d"),
            ((0, 4, 7, 3), "#8c00d9"),
        ]
        faces = sorted(faces, key=lambda f: sum(transformed[i][2] for i in f[0]) / 4.0)
        painter.setPen(QPen(QColor("#050505"), 2))
        for indices, color in faces:
            polygon = QPolygonF([projected[index] for index in indices])
            painter.setBrush(QColor(color))
            painter.drawPolygon(polygon)

        origin = self._project(self._camera_transform((0.0, 0.0, 0.2), apply_board=True), width, height)
        axis_specs = [
            ((1.8, 0.0, 0.2), "#ff2020"),
            ((0.0, 1.4, 0.2), "#1464ff"),
            ((0.0, 0.0, 1.0), "#00df30"),
        ]
        for endpoint, color in axis_specs:
            painter.setPen(QPen(QColor(color), 3))
            p2 = self._project(self._camera_transform(endpoint, apply_board=True), width, height)
            painter.drawLine(origin, p2)

    def _draw_hud(self, painter, width, _height):
        pad = 12
        hud_w = min(500, max(300, width - 2 * pad))
        hud_h = 118

        # Keep the telemetry panel visually stable. Its background never depends
        # on sensor values or on whatever brush was used to draw the 3D model.
        painter.save()
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(QBrush(QColor("#20242c")))
        painter.drawRoundedRect(pad, pad, hud_w, hud_h, 4, 4)
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.setPen(QPen(QColor("#3f4652"), 1))
        painter.drawRoundedRect(pad, pad, hud_w, hud_h, 4, 4)
        painter.restore()

        if self.latest_sample is None:
            lines = ["Waiting for UART data..."]
        else:
            s = self.latest_sample
            ox, oy, oz = self.offset
            lines = [
                f"acc   {s.ax:+.3f}  {s.ay:+.3f}  {s.az:+.3f} g",
                f"gyro  {s.gx:+.3f}  {s.gy:+.3f}  {s.gz:+.3f} deg/s",
                f"quat  {self.q.w:+.3f}  {self.q.x:+.3f}  {self.q.y:+.3f}  {self.q.z:+.3f}",
                f"move  {ox:+.2f}  {oy:+.2f}  {oz:+.2f}",
            ]

        fixed_font = QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont)
        fixed_font.setPointSize(10)
        painter.setFont(fixed_font)
        painter.setPen(QColor("#e8ebf0"))
        for index, line in enumerate(lines):
            painter.drawText(QPointF(pad + 12, pad + 23 + index * 24), line)

    def _draw_legend(self, painter, width, height):
        if height < 360:
            return
        fixed_font = QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont)
        fixed_font.setPointSize(9)
        painter.setFont(fixed_font)
        painter.setPen(QColor("#b2b6bf"))
        painter.drawText(QPointF(14, height - 16), "green +Z   blue +Y   red +X")


class SensorViewerWindow(QMainWindow):
    def __init__(self, uart, port, baud, kp):
        super().__init__()
        self.uart = uart
        self.port = port
        self.baud = baud
        self.connection_text = f"{port} @ {baud} baud"
        self.running = True

        self.estimator = MahonyAttitudeEstimator()
        self.estimator.kp = kp
        self.offset_filter = VisualOffsetFilter()
        self.stats = ProtocolStats()
        self.latest_sample = None
        self.q = Quaternion(1.0, 0.0, 0.0, 0.0)
        self.offset = (0.0, 0.0, 0.0)
        self.messages = deque(maxlen=100)

        self._configure_window()
        self._build_ui()
        self._setup_timer()
        self.log("Connected.")

    def _configure_window(self):
        # QMainWindow uses normal OS window decorations by default. Do not set
        # FramelessWindowHint or any custom title-bar flags here.
        self.setWindowTitle(APP_NAME)
        self.resize(*DEFAULT_SIZE)
        self.setMinimumSize(*MIN_SIZE)

    def _build_ui(self):
        central = QWidget(self)
        root_layout = QVBoxLayout(central)
        root_layout.setContentsMargins(8, 8, 8, 8)
        root_layout.setSpacing(6)

        splitter = QSplitter(Qt.Orientation.Horizontal, central)
        splitter.setChildrenCollapsible(False)

        self.canvas = SensorCanvas(splitter)
        splitter.addWidget(self.canvas)

        side = self._build_side_panel(splitter)
        side.setMinimumWidth(280)
        side.setMaximumWidth(420)
        splitter.addWidget(side)
        splitter.setSizes([800, 320])
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 0)

        root_layout.addWidget(splitter, 1)
        self.setCentralWidget(central)

        status = QStatusBar(self)
        status.setSizeGripEnabled(True)
        self.setStatusBar(status)
        self.statusBar().showMessage(f"{self.connection_text} | waiting for sensor data")

    def _build_side_panel(self, parent):
        side = QFrame(parent)
        layout = QVBoxLayout(side)
        layout.setContentsMargins(10, 4, 4, 4)
        layout.setSpacing(8)

        title = QLabel(APP_NAME)
        title_font = title.font()
        title_font.setPointSize(15)
        title_font.setBold(True)
        title.setFont(title_font)
        layout.addWidget(title)

        layout.addWidget(self._button_group("Mode", [
            ("Low", lambda: self.send_command("mode low")),
            ("Normal", lambda: self.send_command("mode normal")),
            ("High", lambda: self.send_command("mode high")),
        ]))
        layout.addWidget(self._button_group("Stream", [
            ("On", lambda: self.send_command("stream on")),
            ("Off", lambda: self.send_command("stream off")),
        ]))
        layout.addWidget(self._button_group("LED", [
            ("On", lambda: self.send_command("led on")),
            ("Off", lambda: self.send_command("led off")),
        ]))
        layout.addWidget(self._action_group())

        state = QGroupBox("State")
        state_layout = QVBoxLayout(state)
        self.mode_label = QLabel("Mode: unknown")
        self.stream_label = QLabel("Stream: unknown")
        self.led_label = QLabel("LED: unknown")
        state_layout.addWidget(self.mode_label)
        state_layout.addWidget(self.stream_label)
        state_layout.addWidget(self.led_label)
        layout.addWidget(state)

        layout.addWidget(QLabel("Device messages"))
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setMinimumHeight(90)
        fixed_font = QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont)
        fixed_font.setPointSize(9)
        self.log_text.setFont(fixed_font)
        layout.addWidget(self.log_text, 1)
        return side

    def _button_group(self, title, buttons):
        box = QGroupBox(title)
        row = QHBoxLayout(box)
        for label, callback in buttons:
            button = QPushButton(label)
            button.clicked.connect(callback)
            row.addWidget(button)
        return box

    def _action_group(self):
        box = QGroupBox("Actions")
        grid = QGridLayout(box)
        actions = [
            ("Status", lambda: self.send_command("status")),
            ("Help", lambda: self.send_command("help")),
            ("Sensor reset", self.sensor_reset),
            ("View reset", self.local_reset),
            ("Clear log", self.clear_log),
            ("Exit", self.close),
        ]
        for index, (label, callback) in enumerate(actions):
            button = QPushButton(label)
            button.clicked.connect(callback)
            grid.addWidget(button, index // 2, index % 2)
        return box

    def _setup_timer(self):
        self.timer = QTimer(self)
        self.timer.setInterval(16)
        self.timer.timeout.connect(self._tick)
        self.timer.start()

    def keyPressEvent(self, event):
        if event.key() == Qt.Key.Key_Escape:
            self.close()
            return
        if event.key() == Qt.Key.Key_R:
            self.local_reset()
            return
        super().keyPressEvent(event)

    def log(self, text):
        text = str(text)
        self.messages.append(text)
        self.log_text.append(text)
        scrollbar = self.log_text.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

    def clear_log(self):
        self.messages.clear()
        self.log_text.clear()

    def send_command(self, command):
        try:
            self.uart.send_command(command)
        except (serial.SerialException, OSError) as error:
            self.log(f"[Error] Serial: {error}")
            self.close()
            return

        self._apply_optimistic_state(command)
        self.log(f"Sent: {command}")

    def sensor_reset(self):
        self.local_reset(log_message=False)
        self.send_command("reset")

    def local_reset(self, log_message=True):
        self.offset_filter.reset()
        if self.latest_sample is not None:
            self.estimator.reset(self.latest_sample)
            self.q = self.estimator.q
        else:
            self.q = Quaternion(1.0, 0.0, 0.0, 0.0)
        if log_message:
            self.log("Viewer orientation reset.")
        self.canvas.set_scene(self.latest_sample, self.q, self.offset)

    def _apply_optimistic_state(self, command):
        if command.startswith("mode "):
            self.mode_label.setText(f"Mode: {command.split()[1]}")
        elif command == "stream on":
            self.stream_label.setText("Stream: on")
        elif command == "stream off":
            self.stream_label.setText("Stream: off")
        elif command == "led on":
            self.led_label.setText("LED: on")
        elif command == "led off":
            self.led_label.setText("LED: off")

    def _observe_response(self, payload):
        upper = payload.upper()
        for mode in ("LOW", "NORMAL", "HIGH"):
            if "MODE" in upper and mode in upper:
                self.mode_label.setText(f"Mode: {mode.lower()}")
                break
        if "STREAM" in upper:
            if "ON" in upper:
                self.stream_label.setText("Stream: on")
            elif "OFF" in upper:
                self.stream_label.setText("Stream: off")
        if "LED" in upper:
            if "ON" in upper:
                self.led_label.setText("LED: on")
            elif "OFF" in upper:
                self.led_label.setText("LED: off")

    def _tick(self):
        if not self.running:
            return

        try:
            lines = self.uart.poll_lines()
        except (serial.SerialException, OSError) as error:
            self.log(f"[Error] Serial: {error}")
            self.close()
            return

        for line in lines:
            kind, payload = SensorProtocol.parse(line)
            if kind in ("data", "legacy_data"):
                sample, count = payload
                self.latest_sample = sample
                self.stats.record_sample(time.monotonic(), count)
                self.q = self.estimator.update(sample)
            elif kind == "response":
                self._observe_response(payload)
                self.log(f"[Response] {payload}")
            elif kind == "error":
                self.log(f"[Error] {payload}")
            elif kind == "type":
                self.log(f"[Type] {payload}")
            elif kind == "malformed":
                self.log("[Error] Malformed DATA packet")
            elif kind == "invalid":
                self.log("[Error] Invalid DATA values")
            elif kind == "message":
                self.log(payload)

        self.offset = self.offset_filter.update(self.latest_sample, self.q)
        self.canvas.set_scene(self.latest_sample, self.q, self.offset)
        self.statusBar().showMessage(f"{self.connection_text} | {self.stats.timing_text()}")

    def closeEvent(self, event):
        if self.running:
            self.running = False
            self.timer.stop()
            try:
                self.uart.close()
            except (serial.SerialException, OSError):
                pass
        event.accept()


def main():
    parser = argparse.ArgumentParser(
        description="Resizable 3D IMU sensor viewer"
    )
    parser.add_argument("port", help="Serial port, for example COM5 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200, help="UART baud rate (default: 115200)")
    parser.add_argument("--kp", type=float, default=2.0, help="Mahony proportional gain (default: 2.0)")
    args = parser.parse_args()

    uart = UARTClient(args.port, args.baud)
    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    window = SensorViewerWindow(uart, args.port, args.baud, args.kp)
    window.show()
    exit_code = app.exec()
    print("Disconnected.")
    return exit_code


if __name__ == "__main__":
    try:
        sys.exit(main())
    except serial.SerialException as error:
        print(f"Cannot open serial port: {error}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        pass
