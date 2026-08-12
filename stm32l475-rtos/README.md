# STM32L475 Bare-Metal RTOS

A small, statically allocated RTOS for the STM32L475VG (Arm Cortex-M4), built with CMake and the GNU Arm Embedded toolchain. The repository includes on-target integration tests, SEGGER tracing, TeSSLa runtime verification, Doxygen documentation, and GitLab CI.

## Features

- Preemptive fixed-priority scheduling; larger numbers mean higher priority
- FIFO round-robin between equal-priority tasks on SysTick or explicit yield
- Static task table and per-task stacks; no kernel heap allocation
- Blocking and busy-wait delays with wrap-safe finite timeouts
- Non-recursive mutexes and bounded counting semaphores with direct handoff
- Caller-backed, fixed-capacity FIFO message queues with direct handoff
- Priority-ordered waiters with FIFO ordering at equal priority
- Cortex-M context switching through PSP, SVC, PendSV, SysTick, and `BASEPRI`
- SEGGER SystemView and TeSSLa-compatible trace events over RTT

Mutexes do not implement priority inheritance. Tasks must be created before `os_start()` and cannot be deleted. Compile-time limits and trace options are in `Config/os_config.h`.

For a high-level view of the RTOS structure, task-state model, scheduler flow, context switching, synchronization behavior, and trace architecture, see **[ARCHITECTURE.md](ARCHITECTURE.md)**.

## Build

Install CMake, Ninja, and `gcc-arm-none-eabi`, then run:

```bash
cmake --preset Debug
cmake --build --preset Debug
```

The build produces `build/Debug/rtos.elf` and `build/Debug/rtos.map`. Use the `Release` preset for an optimized build.

For complete prerequisites, flashing, GDB debugging, formatting, Doxygen, VS Code, and the local GitLab runner, see **[SETUP.md](../SETUP.md)**.

For SEGGER SystemView setup and recording instructions, including use of the provided `RTOS.SVPrj` project file, see **[SEGGER.md](SEGGER.md)**.

Flash with the supplied J-Link script:

```bash
JLinkExe -device STM32L475VG -if SWD -speed 4000 \
  -CommanderScript scripts/flash.jlink
```

## Integration tests

The firmware builds one on-target scenario at a time. Select it in [`App/Inc/project.h`](App/Inc/project.h):

```c
#define PROJECT PROJECT_QUEUE
```

Available scenarios cover the scheduler, delays, semaphores, mutexes, and queues. Inspect `g_integration_test_result` in the debugger for the verdict. See the [application test guide](App/README.md) for exact checks.

## Runtime verification

TeSSLa monitors check temporal properties that the C assertions cannot prove alone. Modules cover scheduler behavior, delay timing, semaphores, mutexes, queues, and trace integrity.

```bash
cd verification
python3 tessla_verify.py list
```

See the **[verification guide](../verification/README.md)** for RTT capture, monitor generation, fixture tests, recorded-trace verification, and known limitations.

## Layout

```text
.
├── RTOS/Public/     Public API
├── RTOS/Kernel/     Scheduler and kernel services
├── RTOS/Internal/   Lists, wait queues, tracing, and panic handling
├── Port/CortexM/    Cortex-M port
├── Config/          Compile-time configuration
└── App/             Integration-test application
```

Generate API documentation with `doxygen Doxyfile`.