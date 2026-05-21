#ifndef PORT_ASM_H_
#define PORT_ASM_H_

/**
 * @file port_asm.h
 * @brief Assembly entry points for the Cortex-M RTOS port.
 *
 * Implemented in `port_asm.s`.
 */

/**
 * @brief Start the first task by entering the SVC handler.
 *
 * Enables interrupts, executes `svc #0`, and should never return.
 */
void Port_StartFirstTaskAsm(void);

/**
 * @brief SVC exception handler used to start the first task.
 *
 * Restores the first task context from `g_current_tcb`, switches Thread mode
 * to use PSP, and returns into the first task using EXC_RETURN.
 */
void SVC_Handler(void);

/**
 * @brief PendSV exception handler used for task context switching.
 *
 * Saves the outgoing task context, calls the scheduler, restores the incoming
 * task context, and returns to Thread mode.
 */
void PendSV_Handler(void);

#endif