#include "k_panic.h"
#include "port.h"

void k_panic(void) {
    port_halt();
}