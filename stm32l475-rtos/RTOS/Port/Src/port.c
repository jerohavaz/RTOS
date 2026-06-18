#include "port.h"

#include "stm32l475xx.h"
#include "cmsis_gcc.h"

#define PORT_KERNEL_INTERRUPT_PRIORITY 13u

#define PORT_BASEPRI_VALUE (PORT_KERNEL_INTERRUPT_PRIORITY << (8u - __NVIC_PRIO_BITS))

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
    uint32_t previous_basepri = __get_BASEPRI();

    __asm volatile("msr BASEPRI_MAX, %0" : : "r"(PORT_BASEPRI_VALUE) : "memory");

    /*
     * Prevent memory/instruction reordering across the critical-section
     * boundary and ensure the IRQ mask is active before following code runs.
     */
    __DSB();
    __ISB();

    return previous_basepri;
}

void port_exit_critical(uint32_t previous_basepri) {
    __set_BASEPRI(previous_basepri);

    /*
     * Ensure memory operations inside the critical section complete before
     * restoring the previous interrupt state.
     */
    __DSB();
    __ISB();
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

void port_no_operation(void) {
    __NOP();
}

uint32_t port_get_active_exception_id(void) {
    return __get_IPSR();
}