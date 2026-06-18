.syntax unified
.cpu cortex-m4
.thumb

.extern k_sched_start_first_context
.extern k_sched_switch_context

.global Port_StartFirstTaskAsm
.thumb_func
.type Port_StartFirstTaskAsm, %function

Port_StartFirstTaskAsm:
    cpsie i
    dsb
    isb

    svc #0
    bx lr


.global SVC_Handler
.thumb_func
.type SVC_Handler, %function

SVC_Handler:
    /*
     * C scheduler returns first task saved SP.
     * r0 = first task SP before restoring R4-R11.
     */
    bl k_sched_start_first_context
    cbz r0, SVC_Fault

    /* Restore software-saved task context. */
    ldmia r0!, {r4-r11}
    msr psp, r0
    isb

    /*
     * Thread mode uses PSP.
     *
     * CONTROL.SPSEL = 1
     * CONTROL.nPRIV = 0 privileged
     * CONTROL.FPCA = 0 no floating-point context active
     */
    movs r0, #2
    msr CONTROL, r0
    isb

    /*
     * Exception return:
     * 0xFFFFFFFD = return to Thread mode, use PSP, no FP extended frame.
     */
    ldr lr, =0xFFFFFFFD
    bx lr


.global PendSV_Handler
.thumb_func
.type PendSV_Handler, %function

PendSV_Handler:
    /*
     * Save outgoing task software context.
     *
     * Hardware has already stacked R0-R3, R12, LR, PC, xPSR.
     * We save R4-R11 manually.
     */
    mrs r0, psp
    cbz r0, PendSV_Fault

    stmdb r0!, {r4-r11}

    /*
     * r0 = outgoing saved SP.
     *
     * k_sched_switch_context(outgoing_sp):
     *   - stores outgoing SP into old task
     *   - selects next task
     *   - returns incoming saved SP in r0
     *
     * Preserve EXC_RETURN in LR across the C call.
     * Push two registers to keep MSP 8-byte aligned for AAPCS.
     */
    push {r3, lr}
    bl k_sched_switch_context
    pop {r3, lr}

    cbz r0, PendSV_Fault

    /* Restore incoming task software context. */
    ldmia r0!, {r4-r11}
    msr psp, r0
    isb

    bx lr



SVC_Fault:
    bkpt #2
    b .

PendSV_Fault:
    bkpt #1
    b .