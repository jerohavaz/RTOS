# STM32L475 RTOS

A small bare-metal RTOS for the STM32L475VG (Arm Cortex-M4), built with CMake
and the GNU Arm Embedded toolchain. The repository includes the firmware,
SEGGER tracing, TeSSLa runtime verification, Doxygen documentation, and a
GitLab CI pipeline.

## Features

- Static task and stack allocation; no kernel heap allocation
- Preemptive fixed-priority scheduling
- Round-robin scheduling for equal-priority tasks on tick or yield
- Blocking and busy-wait delays with wrap-safe 32-bit timeouts
- Non-recursive mutexes, bounded counting semaphores, and fixed-size queues
- Priority-ordered wait queues with FIFO ordering inside each priority
- Cortex-M context switching through SVC, PendSV, PSP, and SysTick
- SEGGER SystemView and TeSSLa-compatible RTT tracing
- Doxygen API documentation and GitLab CI checks

Mutexes currently do not implement priority inheritance.

## Repository Layout

```text
.
├── stm32l475-rtos/     # Firmware, RTOS kernel, Cortex-M port, and application
├── verification/       # RTT capture and TeSSLa specifications/tests
├── gitlab-runner/      # Local GitLab runner setup
├── .gitlab-ci.yml
└── README.md
```

Run firmware commands from:

```bash
cd stm32l475-rtos
```

## Setup

Install the build and development tools:

```bash
sudo apt update
sudo apt install cmake ninja-build gcc-arm-none-eabi gdb-multiarch \
  clang-format cppcheck doxygen
```

Install the SEGGER J-Link Software and Documentation Pack separately and ensure
these commands are available on `PATH`:

```text
JLinkExe
JLinkGDBServer
```

## Build

```bash
cmake --preset Debug
cmake --build --preset Debug
```

The main artifact is:

```text
build/Debug/rtos.elf
```

For an optimized build, replace `Debug` with `Release`.

## Flash and Debug

Flash the target and start the firmware:

```bash
JLinkExe -device STM32L475VG -if SWD -speed 4000 \
  -CommanderScript scripts/flash.jlink
```

For a GDB session, start the server:

```bash
JLinkGDBServer -device STM32L475VG -if SWD -speed 4000 -port 50000
```

Then connect from another terminal:

```bash
gdb-multiarch build/Debug/rtos.elf \
  -ex "set pagination off" \
  -ex "target extended-remote localhost:50000" \
  -ex "monitor reset 0" \
  -ex "load" \
  -ex "monitor reset 0" \
  -ex "continue"
```

## RTOS Structure

The implementation is split into small layers:

| Path | Responsibility |
| --- | --- |
| `RTOS/Public` | Application-facing task, delay, mutex, semaphore, queue, and ISR APIs |
| `RTOS/Kernel` | Scheduler, task lifecycle, timeout handling, and idle task |
| `RTOS/Internal` | Intrusive lists, priority wait queues, ring buffer, tracing, and panic handling |
| `Port/CortexM` | Cortex-M stack setup, critical sections, SVC startup, and PendSV switching |
| `Config/os_config.h` | Task limits, stack size, priorities, tracing, and exception priorities |
| `App` | Example and stress-test tasks |
| `Core` | STM32 startup, HAL initialization, and interrupt integration |

Higher numeric task priorities run first. Equal-priority tasks rotate on a
yield or SysTick. When no application task is ready, the separately managed
idle task runs.

Kernel objects use caller-provided or static storage. Blocked tasks can wait
forever, return immediately with `OS_NO_WAIT`, or use a finite timeout below
`2^31` ticks so comparisons remain valid across tick-counter wraparound.

## Configuration and Tracing

Edit `Config/os_config.h` to configure task capacity, per-task stack size,
priority levels, exception priorities, trace backends, and trace categories.

Tracing supports:

- SEGGER SystemView events over RTT
- Text events over RTT for TeSSLa verification

The idle task remains awake while tracing is enabled to keep RTT/debug access
responsive.

See [verification/README.md](verification/README.md) for RTT capture, TeSSLa
generation, monitor tests, and recorded-trace verification. The monitors cover
scheduler behavior, delay timing, and trace integrity.

## Formatting and Documentation

Check formatting without changing files:

```bash
find App Config Core Port RTOS -type f \( -name '*.c' -o -name '*.h' \) \
  -print0 | xargs -0 clang-format --dry-run --Werror
```

Apply formatting:

```bash
find App Config Core Port RTOS -type f \( -name '*.c' -o -name '*.h' \) \
  -print0 | xargs -0 clang-format -i
```

Generate and open the Doxygen documentation:

```bash
doxygen Doxyfile
xdg-open docs/html/index.html
```

## CI/CD

GitLab push pipelines check formatting, configure and build the Debug preset,
run `cppcheck`, test the TeSSLa verification tooling when it changes, and
publish Doxygen HTML through GitLab Pages from the default branch. Build
artifacts include ELF, map, and binary files.

The repository also contains Docker definitions for the firmware and
verification CI images. To start the supplied local GitLab runner from the
repository root:

```bash
cd gitlab-runner
docker compose up -d --build
```

## VS Code

Recommended extensions:

- CMake Tools
- clangd, or Microsoft C/C++
- Cortex-Debug

The STM32Cube extension is not required.

Example `.vscode/settings.json`:

```json
{
  "cmake.sourceDirectory": "${workspaceFolder}/stm32l475-rtos",
  "cmake.useCMakePresets": "always",
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}/stm32l475-rtos/build/Debug",
    "--query-driver=/usr/bin/arm-none-eabi-gcc,/usr/bin/arm-none-eabi-g++",
    "--background-index"
  ],
  "C_Cpp.intelliSenseEngine": "disabled"
}
```

Example `.vscode/launch.json`:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug STM32L475",
      "type": "cortex-debug",
      "request": "launch",
      "servertype": "jlink",
      "device": "STM32L475VG",
      "interface": "swd",
      "cwd": "${workspaceFolder}/stm32l475-rtos",
      "executable": "${workspaceFolder}/stm32l475-rtos/build/Debug/rtos.elf",
      "gdbPath": "gdb-multiarch",
      "objdumpPath": "arm-none-eabi-objdump",
      "runToEntryPoint": "main"
    }
  ]
}
```