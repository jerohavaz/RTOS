# RTOS

STM32L475 bare-metal RTOS project built with CMake and debugged through J-Link.

## Repository Layout

```text
.
├── stm32l475-rtos/     # Firmware project
├── gitlab-runner/      # Local GitLab runner setup
└── README.md
```

All firmware commands in this document are run from:

```bash
./stm32l475-rtos
```

## Toolchain Requirements

### Build tools

```bash
sudo apt update
sudo apt install cmake ninja-build gcc-arm-none-eabi gdb-multiarch clang-format doxygen
```

Required commands:

```bash
cmake
ninja
arm-none-eabi-gcc
arm-none-eabi-objdump
arm-none-eabi-nm
gdb-multiarch
clang-format
doxygen
```

### Debug and flash tools

Install SEGGER J-Link tools and make sure these commands are available on `PATH`:

```bash
JLinkExe
JLinkGDBServer
```

The examples assume:

```text
Target MCU: STM32L475VG
Debug probe: J-Link
Interface: SWD
Speed: 4000 kHz
GDB port: 50000
```

## Build Firmware

Configure the Debug preset:

```bash
cmake --preset Debug
```

Build the firmware:

```bash
cmake --build build/Debug
```

Expected output:

```text
build/Debug/rtos.elf
```

## Flash and Run

Use this when you only want to program the board and start the firmware.

```bash
JLinkExe -device STM32L475VG -if SWD -speed 4000 -CommanderScript scripts/flash.jlink
```

## Flash and Debug

Use this when you want to debug with GDB.

Start the J-Link GDB server in one terminal:

```bash
JLinkGDBServer -device STM32L475VG -if SWD -speed 4000 -port 50000
```

In a second terminal, connect GDB, flash the ELF, reset the target, and continue:

```bash
gdb-multiarch build/Debug/rtos.elf \
  -ex "set pagination off" \
  -ex "target extended-remote localhost:50000" \
  -ex "monitor reset 0" \
  -ex "load" \
  -ex "monitor reset 0" \
  -ex "continue"
```

## Format Source Code

Format project-owned C and header files:

```bash
find Core RTOS App Config Port -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 clang-format -i
```

## VS Code Setup

Open the repository root in VS Code if you want access to both firmware and runner files.

Required extensions:

```text
CMake Tools
C/C++ or clangd
Cortex-Debug
```

The STM32Cube extension is not required for building or debugging this project.

### `.vscode/settings.json`

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

### `.vscode/launch.json`

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

## Generate Documentation

Generate Doxygen documentation:

```bash
doxygen Doxyfile
```

Open the generated HTML documentation:

```bash
xdg-open docs/html/index.html
```

## CI/CD Runner

The GitLab runner files are outside the firmware project:

```text
./gitlab-runner
```

Start the local runner:

```bash
cd ../gitlab-runner
docker-compose up -d --build
```
