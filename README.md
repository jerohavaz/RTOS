# STM32L475 Bare-Metal RTOS

Custom bare-metal RTOS for the STM32L475VG with fixed-priority preemptive scheduling, synchronization primitives, message queues, SEGGER SystemView instrumentation, TeSSLa runtime verification, integration tests, a sensor terminal, a sensor viewer, and GitLab CI/CD.

## Documentation

* [RTOS & Firmware](stm32l475-rtos/README.md)
* [Architecture](stm32l475-rtos/ARCHITECTURE.md)
* [Setup & Development](SETUP.md)
* [Integration Tests](stm32l475-rtos/App/README.md)
* [SEGGER SystemView](stm32l475-rtos/SEGGER.md)
* [TeSSLa Verification](verification/README.md)
* [Sensor Terminal](sensor_terminal/README.md)
* [Sensor Viewer](sensor_viewer/README.md)

## GitLab CI/CD

The root `.gitlab-ci.yml` defines the CI/CD pipeline for firmware builds, static analysis, TeSSLa verification, and documentation.

| Job                                                  | Check                                            |
| ---------------------------------------------------- | ------------------------------------------------ |
| `stm32_image`, `doxygen_image`, `verification_image` | Build and publish the required CI Docker images  |
| `format`                                             | Check C and header files using `clang-format`    |
| `build`                                              | Build all firmware configurations in parallel    |
| `cppcheck`                                           | Run static analysis for all build configurations |
| `verification`                                       | Run all TeSSLa verification fixtures             |
| `pages`                                              | Generate and publish the Doxygen documentation   |

The firmware build uses a parallel matrix for:

* `sensor`
* `queue`
* `scheduler`
* `delay`
* `semaphore`
* `mutex`
* `sensor-no-trace`

Each build uses a separate CMake build directory. The generated firmware files and `compile_commands.json` are stored as CI artifacts.

Cppcheck reuses the `compile_commands.json` files from the build jobs, so the project does not need to be configured again for static analysis.

TeSSLa verification runs using the dedicated verification Docker image and executes all available verification fixtures with the Rust monitor backend.

The documentation is generated with Doxygen and published through GitLab Pages on the default branch.

Merge-request-event pipelines are disabled.
