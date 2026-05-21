.syntax unified
.cpu cortex-m4
.thumb

.extern g_current_tcb

.extern k_sched_first_task_started
.extern k_sched_switch

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
    bl k_sched_first_task_started

    /* Load current TCB stack pointer */
    ldr r0, =g_current_tcb
    ldr r0, [r0]
    cbz r0, SVC_Fault

    /* Load currentTCB->sp */
    ldr r0, [r0]
    cbz r0, SVC_Fault

    /* Restore software-saved task context */
    ldmia r0!, {r4-r11}
    msr psp, r0

    /* Switch Thread mode to PSP, FPU off and Privileged */
    movs r0, #2
    msr CONTROL, r0
    isb

    /* Exception-return into first task */
    ldr lr, =0xFFFFFFFD
    bx lr


.global PendSV_Handler
.thumb_func
.type PendSV_Handler, %function

PendSV_Handler:
    /* Save outgoing task context */
    mrs r0, psp
    cbz r0, PendSV_Fault

    stmdb r0!, {r4-r11}

    /* Load current TCB */
    ldr r1, =g_current_tcb
    ldr r1, [r1]
    cbz r1, PendSV_Fault

    /* currentTCB->sp = updated PSP */
    str r0, [r1]

    /* Preserve EXC_RETURN and keep 8-byte stack alignment for the C call. */
    push {r3, lr}
    bl k_sched_switch
    pop {r3, lr}

    /* Load incoming TCB */
    ldr r0, =g_current_tcb
    ldr r0, [r0]
    cbz r0, PendSV_Fault

    /* Load incoming currentTCB->sp */
    ldr r0, [r0]
    cbz r0, PendSV_Fault

    /* Restore incoming task context */
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