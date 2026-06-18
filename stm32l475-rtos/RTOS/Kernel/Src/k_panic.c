#include "k_panic.h"
#include "port.h"

K_NORETURN void k_panic(void) {
    port_halt();
    K_UNREACHABLE();
}