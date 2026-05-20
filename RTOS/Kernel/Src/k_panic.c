#include "k_panic.h"
#include "port.h"

void k_panic(void) {
    port_enter_critical();

    while (1) {
        __asm volatile("bkpt #0");
    }
}