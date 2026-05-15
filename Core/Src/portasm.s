.syntax unified
.cpu cortex-m4
.thumb

.extern g_psCurrentTCB

.extern Scheduler_OnFirstTaskStart
.extern Scheduler_SwitchContext

.global Port_StartFirstTaskAsm
.thumb_func
.type Port_StartFirstTaskAsm, %function

Port_StartFirstTaskAsm:
    cpsie i
    cpsie f
    dsb
    isb

    svc #0
    bx lr


.global SVC_Handler
.thumb_func
.type SVC_Handler, %function

SVC_Handler:
    cpsid i

    bl Scheduler_OnFirstTaskStart

    /* Load current TCB stack pointer */
    ldr r0, =g_psCurrentTCB
    ldr r0, [r0]
    cbz r0, SVC_Fault

    /* Load currentTCB->pu32TaskSP */
    ldr r0, [r0]
    cbz r0, SVC_Fault

    /* Restore software-saved task context */
    ldmia r0!, {r4-r11}
    msr psp, r0

    /* Switch Thread mode to PSP */
    movs r0, #2
    msr CONTROL, r0
    isb

    /* Exception-return into first task */
    ldr lr, =0xFFFFFFFD

    cpsie i
    bx lr


.global PendSV_Handler
.thumb_func
.type PendSV_Handler, %function

PendSV_Handler:
    cpsid i
    
    /* Preserve EXC_RETURN across C call */
    push {r3, lr}

    /* Save outgoing task context */
    mrs r0, psp
    cbz r0, PendSV_Fault

    stmdb r0!, {r4-r11}

    /* Load current TCB */
    ldr r1, =g_psCurrentTCB
    ldr r1, [r1]
    cbz r1, PendSV_Fault

    /* currentTCB->pu32TaskSP = updated PSP */
    str r0, [r1]

    /* Select next task */
    bl Scheduler_SwitchContext

    /* Load incoming TCB */
    ldr r0, =g_psCurrentTCB
    ldr r0, [r0]
    cbz r0, PendSV_Fault

    /* Load incoming currentTCB->pu32TaskSP */
    ldr r0, [r0]
    cbz r0, PendSV_Fault

    /* Restore incoming task context */
    ldmia r0!, {r4-r11}
    msr psp, r0
    isb

    /* Restore EXC_RETURN and return to selected task */
    pop {r3, lr}
    
    cpsie i
    bx lr



SVC_Fault:
    bkpt #2
    b .

PendSV_Fault:
    bkpt #1
    b .