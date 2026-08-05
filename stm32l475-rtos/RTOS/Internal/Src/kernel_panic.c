/**
 * @file kernel_panic.c
 * @brief Kernel fatal-error implementation.
 * @author Jerome
 */

#include "kernel_panic.h"
#include "port.h"

KERNEL_NORETURN void kernel_panic(void) {
    port_halt();
    KERNEL_UNREACHABLE();
}
