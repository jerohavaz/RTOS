# Development Setup

This guide covers the local firmware workflow. Run firmware commands from
`stm32l475-rtos/` unless stated otherwise.

## Prerequisites

Install the build and development tools:

```bash
sudo apt update
sudo apt install cmake ninja-build gcc-arm-none-eabi gdb-multiarch \
  clang-format cppcheck doxygen
```

Install the SEGGER J-Link Software and Documentation Pack separately. Ensure
these commands are available on `PATH`:

```text
JLinkExe
JLinkGDBServer
```

## Build

```bash
cd stm32l475-rtos
cmake --preset Debug
cmake --build --preset Debug
```

The available presets are:

| Preset | Flags | Output |
| --- | --- | --- |
| `Debug` | `-O0 -g3` | `build/Debug/rtos.elf`, `build/Debug/rtos.map` |
| `Release` | `-Os -g0` | `build/Release/rtos.elf`, `build/Release/rtos.map` |

Select the on-target integration scenario in `App/Inc/project.h` before
building:

```c
#define PROJECT PROJECT_QUEUE
```

See [App/README.md](stm32l475-rtos/App/README.md) for the available scenarios
and debugger-visible results.

## Flash

Connect the STM32L475 target through J-Link, then run:

```bash
JLinkExe -device STM32L475VG -if SWD -speed 4000 \
  -CommanderScript scripts/flash.jlink
```

## Debug with GDB

Start the J-Link GDB server:

```bash
JLinkGDBServer -device STM32L475VG -if SWD -speed 4000 -port 50000
```

Connect from another terminal:

```bash
gdb-multiarch build/Debug/rtos.elf \
  -ex "set pagination off" \
  -ex "target extended-remote localhost:50000" \
  -ex "monitor reset 0" \
  -ex "load" \
  -ex "monitor reset 0" \
  -ex "continue"
```

Inspect `g_integration_test_result` for the selected test's state, check count,
and failure count.

## Formatting and static analysis

Check formatting without modifying files:

```bash
find App Config Core Port RTOS -type f \( -name '*.c' -o -name '*.h' \) \
  -print0 | xargs -0 clang-format --dry-run --Werror
```

Apply formatting:

```bash
find App Config Core Port RTOS -type f \( -name '*.c' -o -name '*.h' \) \
  -print0 | xargs -0 clang-format -i
```

Run the same `cppcheck` profile used by CI after configuring the Debug build:

```bash
cppcheck \
  --project=build/Debug/compile_commands.json \
  --enable=warning,style,performance,portability \
  --error-exitcode=1 \
  --suppress=missingIncludeSystem \
  --suppress=preprocessorErrorDirective \
  -iLibs/SEGGER \
  -iCore/Src/syscalls.c
```

## API documentation

Generate Doxygen HTML:

```bash
doxygen Doxyfile
```

Open `docs/html/index.html` in a browser. GitLab CI publishes the same Doxygen
output through Pages from the default branch.

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

## Local GitLab runner

From the repository root:

```bash
cd gitlab-runner
docker compose up -d
```

The supplied runner uses the Docker executor and mounts the host Docker socket.
`config.toml` is instance-specific: replace its GitLab URL and registration
details before use, and do not commit a live runner token.

## Runtime verification

Verification has additional Python, Java, TeSSLa, Rust, and RTT requirements.
See the dedicated [verification guide](verification/README.md).