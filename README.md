# STM32L475 Bare-Metal RTOS

Custom bare-metal RTOS for the STM32L475VG with fixed-priority preemptive scheduling, synchronization primitives, message queues, SEGGER SystemView instrumentation, TeSSLa runtime verification, integration tests, and GitLab CI/CD.

## Documentation

* [RTOS & Firmware](stm32l475-rtos/README.md)
* [Architecture](stm32l475-rtos/ARCHITECTURE.md)
* [Setup & Development](SETUP.md)
* [Integration Tests](stm32l475-rtos/App/README.md)
* [SEGGER SystemView](stm32l475-rtos/SEGGER.md)
* [TeSSLa Verification](verification/README.md)

## GitLab CI/CD

The root `.gitlab-ci.yml` defines:

| Job | Check |
| --- | --- |
| `stm32_image`, `doxygen_image`, `verification_image` | Build and publish the CI Docker images when relevant files change; manual fallback |
| `format` | Run `clang-format --dry-run --Werror` on firmware C and header files |
| `build` | Build all five integration-test variants with tracing enabled, plus `queue-no-trace` |
| `cppcheck` | Run static analysis on the queue configuration |
| `verification` | Run all TeSSLa verification fixtures using the Rust monitor backend |
| `pages` | Generate and publish the Doxygen HTML documentation from the default branch |

The firmware build runs as a parallel matrix for `queue`, `scheduler`, `delay`, `semaphore`, and `mutex`. `queue-no-trace` additionally verifies that the firmware compiles with tracing disabled.

TeSSLa verification runs when files below `verification/` or `.gitlab-ci.yml` change. GitLab Pages runs only on the default branch. Merge-request-event pipelines are disabled.
