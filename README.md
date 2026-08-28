# STM32L475 Bare-Metal RTOS

[![RTOS CI](https://github.com/jerohavaz/RTOS/actions/workflows/ci.yml/badge.svg)](https://github.com/jerohavaz/RTOS/actions/workflows/ci.yml)
[![CI Images](https://github.com/jerohavaz/RTOS/actions/workflows/images.yml/badge.svg)](https://github.com/jerohavaz/RTOS/actions/workflows/images.yml)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue)](https://jerohavaz.github.io/RTOS/)

Custom bare-metal RTOS for the STM32L475VG with fixed-priority preemptive scheduling, synchronization primitives, message queues, SEGGER SystemView instrumentation, TeSSLa runtime verification, integration tests, a sensor terminal, and a sensor viewer.

Originally developed for the **Echtzeitsysteme** module at **Technische Hochschule Mittelhessen (THM)**. This repository was imported from the original GitLab project and adapted to support both GitLab CI/CD and GitHub Actions.

## Documentation

* [Doxygen Documentation](https://jerohavaz.github.io/RTOS/)
* [RTOS & Firmware](stm32l475-rtos/README.md)
* [Architecture](stm32l475-rtos/ARCHITECTURE.md)
* [Setup & Development](SETUP.md)
* [Integration Tests](stm32l475-rtos/App/README.md)
* [SEGGER SystemView](stm32l475-rtos/SEGGER.md)
* [TeSSLa Verification](verification/README.md)
* [Sensor Terminal](sensor_terminal/README.md)
* [Sensor Viewer](sensor_viewer/README.md)

## CI/CD

The project supports both **GitLab CI/CD** and **GitHub Actions** for firmware builds, static analysis, TeSSLa verification, Docker image builds, and documentation.

| Job | Check |
| --- | --- |
| CI images | Build the required STM32, Doxygen, and verification Docker images |
| `format` | Check C and header files using `clang-format` |
| `build` | Build all firmware configurations in parallel |
| `cppcheck` | Run static analysis for all build configurations |
| `verification` | Run all TeSSLa verification fixtures |
| documentation | Generate and publish the Doxygen documentation |

The firmware build uses a parallel matrix for:

* `sensor`
* `queue`
* `scheduler`
* `delay`
* `semaphore`
* `mutex`
* `sensor-no-trace`

Each build uses a separate CMake build directory. Generated firmware files and `compile_commands.json` are stored as CI artifacts.

Cppcheck reuses the generated `compile_commands.json` files, avoiding a second project configuration step for static analysis.

TeSSLa verification runs using the dedicated verification Docker image and executes all available verification fixtures with the Rust monitor backend.

Documentation is generated with Doxygen and published through **GitLab Pages** or **GitHub Pages**, depending on the platform.
