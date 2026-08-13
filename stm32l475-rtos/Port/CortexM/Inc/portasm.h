/**
 * @file portasm.h
 * @brief Assembly entry points for Cortex-M4 task startup and context switching.
 * @author Jerome
 *
 * @details
 * This header provides the Doxygen documentation for the implementation in
 * @c portasm.s because the project's Doxygen configuration does not parse the
 * assembly source. See @ref portasm_implementation for the complete assembly
 * behavior and scheduler ABI.
 */

#ifndef PORTASM_H_
#define PORTASM_H_

/**
 * @page portasm_implementation Cortex-M4 assembly port implementation
 *
 * @section portasm_scope Purpose and target
 *
 * @c portasm.s implements first-task startup and task context switching for
 * the ARM Cortex-M4 RTOS port. The file selects:
 *
 * - GNU unified assembly syntax with @c .syntax @c unified.
 * - Cortex-M4 instruction generation with @c .cpu @c cortex-m4.
 * - Thumb instruction state with @c .thumb.
 *
 * The implementation exports @ref Port_StartFirstTaskAsm, @ref SVC_Handler,
 * and @ref PendSV_Handler as Thumb functions.
 *
 * @section portasm_scheduler_abi Scheduler interface
 *
 * The assembly imports two C scheduler functions:
 *
 * - @c k_sched_start_first_context(): returns the first task's saved stack
 *   pointer in R0.
 * - @c k_sched_switch_context(outgoing_sp): receives the outgoing task's saved
 *   stack pointer in R0 and returns the incoming task's saved stack pointer in
 *   R0.
 *
 * Both returned pointers must be non-null and must identify a stack layout
 * compatible with the context described below.
 *
 * @section portasm_stack_layout Task stack layout
 *
 * Cortex-M exception entry automatically stores the hardware exception frame:
 *
 * | Increasing address | Register |
 * |:-------------------|:---------|
 * | lowest             | R0       |
 * |                    | R1       |
 * |                    | R2       |
 * |                    | R3       |
 * |                    | R12      |
 * |                    | LR       |
 * |                    | PC       |
 * | highest            | xPSR     |
 *
 * The assembly port stores R4-R11 immediately below that frame using
 * @c stmdb, producing this software-saved region:
 *
 * | Increasing address | Register |
 * |:-------------------|:---------|
 * | lowest             | R4       |
 * |                    | R5       |
 * |                    | R6       |
 * |                    | R7       |
 * |                    | R8       |
 * |                    | R9       |
 * |                    | R10      |
 * | highest            | R11      |
 *
 * A saved task stack pointer therefore points at R4. Restoring R4-R11 with
 * @c ldmia advances the pointer to the hardware exception frame; that value is
 * then written to PSP.
 *
 * @section portasm_first_task First-task startup
 *
 * @ref Port_StartFirstTaskAsm performs the following sequence:
 *
 * 1. @c cpsie @c i clears PRIMASK and enables normal maskable interrupts.
 * 2. @c dsb and @c isb synchronize the interrupt-state change.
 * 3. @c svc @c 0 enters @ref SVC_Handler.
 * 4. The trailing @c bx @c lr is defensive; normal startup returns from SVC
 *    directly into the first task and never resumes this function.
 *
 * @section portasm_svc SVC first-context restore
 *
 * @ref SVC_Handler:
 *
 * 1. Calls @c k_sched_start_first_context().
 * 2. Branches to @c SVC_Fault if R0 is null.
 * 3. Restores R4-R11 from the address in R0 using @c ldmia @c r0!.
 * 4. Writes the advanced R0 to PSP so PSP points at the hardware frame.
 * 5. Executes @c isb after changing the active process stack.
 * 6. Writes 2 to CONTROL:
 *    - CONTROL.SPSEL = 1 selects PSP in Thread mode.
 *    - CONTROL.nPRIV = 0 keeps Thread mode privileged.
 *    - CONTROL.FPCA = 0 indicates no active floating-point context.
 * 7. Executes @c isb so the CONTROL change takes effect before return.
 * 8. Loads LR with @c 0xFFFFFFFD and branches to LR.
 *
 * @c 0xFFFFFFFD is the Cortex-M exception-return value for privileged or
 * unprivileged Thread mode using PSP with a basic, non-extended exception
 * frame. The restored CONTROL value makes the selected task privileged.
 *
 * @section portasm_pendsv PendSV context switch
 *
 * Hardware exception entry has already saved R0-R3, R12, LR, PC, and xPSR on
 * the outgoing task's PSP before @ref PendSV_Handler begins. The handler:
 *
 * 1. Reads PSP into R0 and branches to @c PendSV_Fault if it is null.
 * 2. Saves R4-R11 below the hardware frame with @c stmdb @c r0!.
 * 3. Pushes R3 and LR on MSP. LR contains the exception-return token; pushing
 *    two registers both preserves it across the C call and keeps MSP
 *    8-byte-aligned for AAPCS.
 * 4. Calls @c k_sched_switch_context() with the outgoing saved SP in R0.
 * 5. Restores R3 and LR from MSP.
 * 6. Branches to @c PendSV_Fault if the returned incoming SP in R0 is null.
 * 7. Restores the incoming task's R4-R11 using @c ldmia @c r0!.
 * 8. Writes the advanced R0 to PSP and executes @c isb.
 * 9. Branches to the preserved LR, causing exception return through the
 *    incoming hardware frame.
 *
 * @section portasm_faults Fault loops
 *
 * The assembly uses two non-returning diagnostic loops:
 *
 * - @c SVC_Fault executes @c bkpt @c 2 and then branches to itself when the
 *   scheduler returns a null first-task stack pointer.
 * - @c PendSV_Fault executes @c bkpt @c 1 and then branches to itself when
 *   either the outgoing PSP or incoming task stack pointer is null.
 *
 * @section portasm_constraints Constraints
 *
 * - Task stack pointers and MSP must satisfy the 8-byte AAPCS alignment rule.
 * - The C scheduler functions must follow AAPCS and preserve callee-saved
 *   registers as required by the ABI.
 * - The initial context produced by @c port_init_task_stack() must match the
 *   R4-R11 plus hardware-frame layout documented above.
 * - Only the core integer context is preserved. The implementation neither
 *   detects extended exception frames nor saves S16-S31; floating-point task
 *   context is unsupported.
 */

/**
 * @brief Start the first task by entering the SVC handler.
 *
 * Enables normal maskable interrupts, executes @c svc 0, and transfers control
 * to @ref SVC_Handler.
 *
 * @pre The scheduler must have selected a first task with a valid initial
 *      stack frame.
 * @post Normal execution continues in the selected task.
 *
 * @note This function does not return during normal operation.
 */
void Port_StartFirstTaskAsm(void);

/**
 * @brief SVC exception handler used to start the first task.
 *
 * Calls @c k_sched_start_first_context() to obtain the first task's saved
 * stack pointer. The handler restores R4-R11, installs the remaining hardware
 * frame on PSP, configures privileged Thread mode to use PSP, and performs an
 * exception return with @c EXC_RETURN value @c 0xFFFFFFFD.
 *
 * @pre @c k_sched_start_first_context() must return a non-null, correctly
 *      aligned pointer to the software-saved R4-R11 context.
 *
 * @note A null stack pointer triggers the assembly fault loop with
 *       breakpoint immediate value 2.
 * @note The selected @c EXC_RETURN value expects a basic frame without an
 *       extended floating-point context.
 */
void SVC_Handler(void);

/**
 * @brief PendSV exception handler used for task context switching.
 *
 * Reads the outgoing task's PSP and saves R4-R11 below the hardware exception
 * frame. It then calls @c k_sched_switch_context(), passing the complete saved
 * stack pointer in R0. The stack pointer returned in R0 identifies the incoming
 * task context; R4-R11 and PSP are restored before exception return.
 *
 * The handler preserves LR, which contains @c EXC_RETURN, across the scheduler
 * call. It pushes two registers so MSP remains 8-byte aligned at the C call
 * boundary as required by AAPCS.
 *
 * @pre PSP must identify a valid outgoing task exception frame.
 * @pre @c k_sched_switch_context() must return a non-null, correctly aligned
 *      incoming task stack pointer.
 *
 * @note A null outgoing or incoming stack pointer triggers the assembly fault
 *       loop with breakpoint immediate value 1.
 */
void PendSV_Handler(void);

#endif /* PORTASM_H_ */