#include "port.h"
#include "stm32l475xx.h"
#include "cmsis_gcc.h"

extern void Port_StartFirstTaskAsm(void);

void port_start_first_task(void) {
    Port_StartFirstTaskAsm();
}

void port_request_context_switch(void) {
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
}

uint32_t port_enter_critical(void) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

void port_exit_critical(uint32_t primask) {
    __set_PRIMASK(primask);
}

void port_wait_for_interrupt(void) {
    __WFI();
}