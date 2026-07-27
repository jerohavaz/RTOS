#include "port.h"
#include "os_config.h"
#include "stm32l475xx.h"

#if (OS_KERNEL_INTERRUPT_PRIORITY >= (1u << __NVIC_PRIO_BITS))
#error "OS_KERNEL_INTERRUPT_PRIORITY is out of range"
#endif

#if (OS_KERNEL_INTERRUPT_PRIORITY == 0u)
#error "OS_KERNEL_INTERRUPT_PRIORITY must not be 0 when using BASEPRI"
#endif

#if (OS_PENDSV_INTERRUPT_PRIORITY >= (1u << __NVIC_PRIO_BITS))
#error "OS_PENDSV_INTERRUPT_PRIORITY is out of range"
#endif

#if (OS_SYSTICK_INTERRUPT_PRIORITY >= (1u << __NVIC_PRIO_BITS))
#error "OS_SYSTICK_INTERRUPT_PRIORITY is out of range"
#endif

#if (OS_SVC_INTERRUPT_PRIORITY >= (1u << __NVIC_PRIO_BITS))
#error "OS_SVC_INTERRUPT_PRIORITY is out of range"
#endif

#define PORT_BASEPRI_VALUE (OS_KERNEL_INTERRUPT_PRIORITY << (8u - __NVIC_PRIO_BITS))

/**
 * @brief Architecture-specific assembly entry for starting the first task.
 *
 * Implemented in the Cortex-M portasm.s file.
 */
extern void Port_StartFirstTaskAsm(void);

/* -------------------------------------------------------------------------- */
/* Scheduler / context switching                                               */
/* -------------------------------------------------------------------------- */

void port_init_scheduler_interrupts(void) {
    NVIC_SetPriority(PendSV_IRQn, OS_PENDSV_INTERRUPT_PRIORITY);
    NVIC_SetPriority(SysTick_IRQn, OS_SYSTICK_INTERRUPT_PRIORITY);
    NVIC_SetPriority(SVCall_IRQn, OS_SVC_INTERRUPT_PRIORITY);
}

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

port_stack_t *port_init_task_stack(port_stack_t *stack_base,
                                   uint32_t stack_words,
                                   port_task_entry_t entry,
                                   port_task_exit_t exit_handler) {
    port_stack_t *sp;

    if ((stack_base == 0) || (stack_words == 0u) || (entry == 0) || (exit_handler == 0)) {
        return 0;
    }

    sp = &stack_base[stack_words];

    /*
     * Cortex-M exception frames require 8-byte stack alignment.
     */
    sp = (port_stack_t *)((uintptr_t)sp & ~(uintptr_t)0x7u);

    /*
     * Hardware-stacked exception frame.
     *
     * This is the frame the CPU expects to find when returning from an
     * exception into thread mode.
     */
    *(--sp) = 0x01000000u;                           /* xPSR */
    *(--sp) = (port_stack_t)(uintptr_t)entry;        /* PC */
    *(--sp) = (port_stack_t)(uintptr_t)exit_handler; /* LR */
    *(--sp) = 0x12121212u;                           /* R12 */
    *(--sp) = 0x03030303u;                           /* R3 */
    *(--sp) = 0x02020202u;                           /* R2 */
    *(--sp) = 0x01010101u;                           /* R1 */
    *(--sp) = 0x00000000u;                           /* R0 */

    /*
     * Software-saved callee registers.
     *
     * These are restored by the PendSV context restore path before exception
     * return.
     */
    *(--sp) = 0x11111111u; /* R11 */
    *(--sp) = 0x10101010u; /* R10 */
    *(--sp) = 0x09090909u; /* R9 */
    *(--sp) = 0x08080808u; /* R8 */
    *(--sp) = 0x07070707u; /* R7 */
    *(--sp) = 0x06060606u; /* R6 */
    *(--sp) = 0x05050505u; /* R5 */
    *(--sp) = 0x04040404u; /* R4 */

    return sp;
}

/* -------------------------------------------------------------------------- */
/* Interrupt control / critical sections                                       */
/* -------------------------------------------------------------------------- */

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

uint32_t port_get_active_exception_id(void) {
    return __get_IPSR();
}

bool port_in_exception(void) {
    return (port_get_active_exception_id() != 0u);
}

/* -------------------------------------------------------------------------- */
/* Idle / debug                                                                */
/* -------------------------------------------------------------------------- */

void port_wait_for_interrupt(void) {
    __WFI();
}

void port_no_operation(void) {
    __NOP();
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