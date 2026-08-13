# STM32 LSM6DSL Test Terminal

`terminal.py` connects to the STM32 UART, displays the latest averaged
accelerometer/gyroscope values, and provides an interactive command shell.

## Setup

Create a virtual environment and install the dependencies:

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install --upgrade pip
pip install -r requirements.txt
```

On Windows PowerShell, activate the environment with:

```powershell
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

Run the terminal using the board's serial port:

```bash
python3 terminal.py /dev/ttyACM0
```

Use a different baud rate only if the firmware is configured differently:

```bash
python3 terminal.py /dev/ttyACM0 --baud 115200
```

## Commands

Type commands at the `cmd>` prompt. Press `Tab` for completion.

```text
help
status
mode low
mode normal
mode high
stream on
stream off
reset
led on
led off
quit
```

Example:

```text
cmd> mode high
[Response] MODE,HIGH,OK

cmd> status
[Response] STATUS,CTRL1_XL=0x60,CTRL2_G=0x6C,IRQ=842,READ=820,DROPPED=0
```

Sensor values are shown in the bottom toolbar:

```text
ACC: -0.015 +0.202 +0.981 g | GYRO: +2.450 -0.777 -0.707 deg/s | samples= 41 | age=   0.0 s | avg(10)= 100.1 ms | error=  +0.1 ms | total=      15.4 s
```

- `samples`: raw sensor readings averaged into the current UART record.
- `age`: seconds since the newest UART record arrived.
- `avg(10)`: average interval between the last ten UART records.
- `error`: difference between the average interval and the 100 ms target.
- `total`: seconds elapsed since the first valid UART record.

## Verification

Enable streaming and test every mode:

```text
stream on
mode low
mode normal
mode high
```

With one UART record every 100 ms, the expected sample counts are:

| Mode | Sensor rate | Expected samples per record |
| --- | ---: | ---: |
| Low | 52 Hz | 5-6 |
| Normal | 104 Hz | 10-11 |
| High | 416 Hz | 40-42 |

The test passes when:

- A new record is displayed approximately every 100 ms.
- `avg(10)` remains close to 100 ms.
- `IRQ` and `READ` increase when `status` is requested.
- `DROPPED` remains `0` during normal operation.
- Mode commands return `MODE,<mode>,OK`.
- `reset` returns `RESET,OK` and streaming resumes.
- No `[Malformed data]`, `[Invalid data]`, `[Error]`, or `[Serial error]`
  messages appear.

Use `quit` or `Ctrl+C` to close the terminal cleanly.
