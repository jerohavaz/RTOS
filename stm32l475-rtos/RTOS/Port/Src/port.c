#include "port.h"

#include "stm32l475xx.h"
#include "cmsis_gcc.h"

/**
 * @brief Architecture-specific assembly entry for starting the first task.
 *
 * Implemented in the Cortex-M portasm.s file.
 */
extern void Port_StartFirstTaskAsm(void);

void port_start_first_task(void) {
    Port_StartFirstTaskAsm();

    /*
     * Starting the first task should never return. If it does, trap here.
     */
    port_halt();
}

void port_request_context_switch(void) {
    /*
     * Pend PendSV.
     *
     * Writing 1 to PENDSVSET requests PendSV. Other bits are unaffected
     * because write-1 semantics apply to this field.
     */
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;

    /*
     * Ensure the PendSV request is visible before execution continues.
     */
    __DSB();
    __ISB();
}

void port_disable_interrupts(void) {
    __disable_irq();

    /*
     * Ensure the interrupt mask is active before following instructions run.
     */
    __DSB();
    __ISB();
}

uint32_t port_enter_critical(void) {
    uint32_t primask = __get_PRIMASK();

    __disable_irq();

    /*
     * Prevent memory/instruction reordering across the critical-section
     * boundary and ensure the IRQ mask is active before following code runs.
     */
    __DSB();
    __ISB();

    return primask;
}

void port_exit_critical(uint32_t primask) {
    /*
     * Ensure memory operations inside the critical section complete before
     * restoring the previous interrupt state.
     */
    __DMB();

    __set_PRIMASK(primask);
}

void port_breakpoint(void) {
    __BKPT(0);
}

void port_halt(void) {
    port_disable_interrupts();

    while (1) {
        port_breakpoint();
    }
}

void port_wait_for_interrupt(void) {
    __WFI();
}

uint32_t port_get_active_exception_id(void) {
    return __get_IPSR();
}