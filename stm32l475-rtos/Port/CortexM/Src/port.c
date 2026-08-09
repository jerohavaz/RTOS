/**
 * @file port.c
 * @brief STM32L475 Cortex-M4 implementation of the RTOS port interface.
 * @author Jerome
 *
 * @details
 * This module configures the kernel exceptions, constructs initial task stack
 * frames, controls interrupt masking, and provides the low-level idle and
 * debug primitives declared in @c port.h. Context save and restore are
 * implemented in @c portasm.s.
 *
 * Critical sections use BASEPRI rather than PRIMASK so interrupts with
 * numerically lower, higher-urgency priorities can continue to execute.
 *
 * @warning The current context-switch assembly preserves only the core integer
 *          registers. Floating-point task context is not supported.
 */

#include "port.h"
#include "os_config.h"
#include "stm32l475xx.h"

/**
 * @brief Number of 32-bit words in the initial core-register context.
 *
 * The frame contains eight hardware-stacked words (R0-R3, R12, LR, PC, xPSR)
 * and eight software-saved words (R4-R11).
 */
#define PORT_INITIAL_CONTEXT_WORDS (16u)

/**
 * @brief Validate that the configured kernel BASEPRI threshold is representable.
 */
#if (OS_KERNEL_INTERRUPT_PRIORITY >= (1u << __NVIC_PRIO_BITS))
#error "OS_KERNEL_INTERRUPT_PRIORITY is out of range"
#endif

/**
 * @brief Reject priority zero because BASEPRI cannot mask it.
 */
#if (OS_KERNEL_INTERRUPT_PRIORITY == 0u)
#error "OS_KERNEL_INTERRUPT_PRIORITY must not be 0 when using BASEPRI"
#endif

/**
 * @brief Validate the configured PendSV logical priority.
 */
#if (OS_PENDSV_INTERRUPT_PRIORITY >= (1u << __NVIC_PRIO_BITS))
#error "OS_PENDSV_INTERRUPT_PRIORITY is out of range"
#endif

/**
 * @brief Validate the configured SysTick logical priority.
 */
#if (OS_SYSTICK_INTERRUPT_PRIORITY >= (1u << __NVIC_PRIO_BITS))
#error "OS_SYSTICK_INTERRUPT_PRIORITY is out of range"
#endif

/**
 * @brief Validate the configured SVC logical priority.
 */
#if (OS_SVC_INTERRUPT_PRIORITY >= (1u << __NVIC_PRIO_BITS))
#error "OS_SVC_INTERRUPT_PRIORITY is out of range"
#endif

/**
 * @brief Hardware-aligned BASEPRI value used by kernel critical sections.
 */
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
    uintptr_t stack_begin;
    uintptr_t stack_end;

    if ((stack_base == 0) || (stack_words == 0u) || (entry == 0) || (exit_handler == 0)) {
        return 0;
    }

    stack_begin = (uintptr_t)stack_base;

    if ((uintptr_t)stack_words > ((UINTPTR_MAX - stack_begin) / sizeof(port_stack_t))) {
        return 0;
    }

    stack_end = stack_begin + ((uintptr_t)stack_words * sizeof(port_stack_t));

    /*
     * Cortex-M exception frames require 8-byte stack alignment.
     */
    stack_end &= ~(uintptr_t)0x7u;

    /*
     * Alignment may discard one word from a 4-byte-aligned buffer. Validate
     * the usable byte range before writing the 16-word initial context.
     */
    if ((stack_end < stack_begin) ||
        ((stack_end - stack_begin) < (PORT_INITIAL_CONTEXT_WORDS * sizeof(port_stack_t)))) {
        return 0;
    }

    sp = (port_stack_t *)stack_end;

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