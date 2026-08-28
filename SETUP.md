# Development Setup

This guide covers the local firmware development workflow for `stm32l475-rtos`.

Unless stated otherwise, run firmware commands from:

```text
stm32l475-rtos/
```

## Prerequisites

Install the required build and development tools:

```bash
sudo apt update
sudo apt install cmake ninja-build gcc-arm-none-eabi gdb-multiarch \
  clang-format cppcheck doxygen
```

Install the **SEGGER J-Link Software and Documentation Pack** separately.

Ensure the following commands are available on `PATH`:

```text
JLinkExe
JLinkGDBServer
```

## Quick Start

Configure and build the Debug firmware:

```bash
cd stm32l475-rtos
cmake --preset Debug
cmake --build --preset Debug
```

The resulting firmware image is:

```text
build/Debug/rtos.elf
```

Before building, select the desired on-target integration scenario in:

```text
App/Inc/project.h
```

For example:

```c
#define PROJECT PROJECT_QUEUE
```

See [stm32l475-rtos/App/README.md](stm32l475-rtos/App/README.md) for the available scenarios and debugger-visible results.

## STM32CubeIDE

STM32CubeIDE can import the existing CMake project directly. Do not create a separate Cube-generated firmware project.

### Import the project

1. Open **File → STM32 Project Create/Import**.
2. Under **Import STM32 Project**, select **STM32 CMake Project** and click **Next**.
3. Enter a project name.
4. Set **Source Directory** to the `stm32l475-rtos` directory and click **Next**.
5. Under **STM32 Device**, click **Select**.
6. Select **STM32L475VGTx** as the target MCU.
7. Click **Finish**.

### Configure debugging

Configure the project to use the **SEGGER J-Link Debugger**.

Use:

```text
Debug interface: SWD
J-Link speed:    8000 kHz
```

The project should continue to use the repository's existing CMake configuration and source tree.

## Command-Line Workflow

### Build

The project provides separate Debug and Release CMake presets.

```bash
cmake --preset Debug
cmake --build --preset Debug
```

or:

```bash
cmake --preset Release
cmake --build --preset Release
```

| Preset    | Compiler flags | Output                                             |
| --------- | -------------- | -------------------------------------------------- |
| `Debug`   | `-O0 -g3`      | `build/Debug/rtos.elf`, `build/Debug/rtos.map`     |
| `Release` | `-Os -g0`      | `build/Release/rtos.elf`, `build/Release/rtos.map` |

### Flash with J-Link

Connect the STM32L475 target through J-Link, then run:

```bash
JLinkExe -device STM32L475VG -if SWD -speed 4000 \
  -CommanderScript scripts/flash.jlink
```

### Debug with GDB

Start the J-Link GDB server:

```bash
JLinkGDBServer -device STM32L475VG -if SWD -speed 4000 -port 50000
```

From another terminal, connect GDB:

```bash
gdb-multiarch build/Debug/rtos.elf \
  -ex "set pagination off" \
  -ex "target extended-remote localhost:50000" \
  -ex "monitor reset 0" \
  -ex "load" \
  -ex "monitor reset 0" \
  -ex "continue"
```

For integration scenarios, inspect:

```text
g_integration_test_result
```

This exposes the selected test's state, check count, and failure count.

## Formatting and Static Analysis

### Check formatting

```bash
find App Config Core Port RTOS -type f \( -name '*.c' -o -name '*.h' \) \
  -print0 | xargs -0 clang-format --dry-run --Werror
```

### Apply formatting

```bash
find App Config Core Port RTOS -type f \( -name '*.c' -o -name '*.h' \) \
  -print0 | xargs -0 clang-format -i
```

### Run static analysis

Configure the Debug build first so that `compile_commands.json` is available, then run the same `cppcheck` profile used by CI:

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

## API Documentation

Generate the Doxygen documentation with:

```bash
doxygen Doxyfile
```

Open:

```text
docs/html/index.html
```

GitLab CI publishes the same Doxygen output through Pages from the default branch.

## VS Code

Recommended extensions:

* CMake Tools
* clangd, or Microsoft C/C++
* Cortex-Debug

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

## Runtime Verification

Runtime verification has additional Python, Java, TeSSLa, Rust, and RTT requirements.

See the dedicated [verification guide](verification/README.md).
