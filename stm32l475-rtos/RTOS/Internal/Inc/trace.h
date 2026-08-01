/**
 * @file trace.h
 * @brief Internal kernel trace-event interface.
 * @author Jerome
 *
 * @details
 * Decouples kernel instrumentation sites from the configured trace backends.
 * When @c OS_TRACE_ENABLED is true, calls are implemented by @c trace.c and
 * routed according to the backend and category switches in @c os_config.h.
 * The current implementation supports SEGGER SystemView and a
 * Tessla-compatible text stream over SEGGER RTT.
 *
 * When tracing is disabled, this header supplies type-safe inline no-op
 * functions. Kernel call sites therefore require no conditional compilation
 * and generate no external trace dependency.
 *
 * @warning Trace functions may execute from scheduler critical sections and
 *          interrupt context. Enabled backends must be safe in those contexts
 *          and must not call blocking RTOS services.
 */

#pragma once

#include "os_config.h"
#include "tcb.h"

#include <stdint.h>

#if OS_TRACE_ENABLED

/**
 * @brief Initialize every enabled trace backend.
 *
 * The RTT backend is initialized and emits @c TESSLA_START. SystemView is
 * configured and receives a startup message.
 *
 * @pre Call once during @c os_init() before task creation and scheduler start.
 */
void trace_init(void);

/* --------------------------------------------------------------------------
 * Task events
 * -------------------------------------------------------------------------- */

/**
 * @brief Record creation of a task.
 *
 * @param task Initialized task control block containing a valid ID and
 *             priority.
 *
 * @pre @p task must not be null.
 *
 * @note Emitted only when @c OS_TRACE_TASKS is enabled.
 */
void trace_task_create(TCB_sctTCB_t *task);

/**
 * @brief Record a task-state transition.
 *
 * @param task_id Numeric ID of the affected task.
 * @param old_state Previous task-state value.
 * @param new_state New task-state value.
 *
 * @note The current implementation emits this event only through the RTT text
 *       backend when @c OS_TRACE_TASKS is enabled.
 */
void trace_task_state(uint8_t task_id, uint8_t old_state, uint8_t new_state);

/* --------------------------------------------------------------------------
 * Scheduler events
 * -------------------------------------------------------------------------- */

/**
 * @brief Record that a task entered the ready state.
 *
 * @param task Task control block entering the ready state.
 *
 * @pre @p task must not be null.
 * @note Emitted only when @c OS_TRACE_SCHEDULER is enabled.
 */
void trace_task_ready(TCB_sctTCB_t *task);

/**
 * @brief Record that the scheduler started executing a task.
 *
 * @param task Task control block entering the running state.
 *
 * @pre @p task must not be null.
 * @note Emitted only when @c OS_TRACE_SCHEDULER is enabled.
 */
void trace_task_run(TCB_sctTCB_t *task);

/**
 * @brief Record that execution of the current task stopped.
 *
 * @note Emitted only when @c OS_TRACE_SCHEDULER is enabled.
 */
void trace_task_stop_run(void);

/**
 * @brief Record that a task entered the blocked state.
 *
 * @param task Task control block entering the blocked state.
 *
 * @pre @p task must not be null.
 * @note Emitted only when @c OS_TRACE_SCHEDULER is enabled.
 */
void trace_task_block(TCB_sctTCB_t *task);

/**
 * @brief Record that the scheduler selected the idle task.
 *
 * @note Emitted only when @c OS_TRACE_SCHEDULER is enabled.
 */
void trace_idle(void);

/**
 * @brief Record advancement of the kernel tick.
 *
 * @param dt Number of elapsed kernel ticks represented by the event.
 *
 * @note The current implementation emits this event through the RTT text
 *       backend when @c OS_TRACE_SCHEDULER is enabled.
 */
void trace_tick(uint32_t dt);

/* --------------------------------------------------------------------------
 * ISR events
 * -------------------------------------------------------------------------- */

/**
 * @brief Record entry into an interrupt service routine.
 *
 * @note Routed to SystemView when @c OS_TRACE_SEGGER_SYSVIEW and
 *       @c OS_TRACE_ISR are enabled.
 */
void trace_isr_enter(void);

/**
 * @brief Record an ISR exit that resumes normal interrupted execution.
 *
 * @note Routed to SystemView when @c OS_TRACE_SEGGER_SYSVIEW and
 *       @c OS_TRACE_ISR are enabled.
 */
void trace_isr_exit(void);

/**
 * @brief Record an ISR exit that transfers control to the scheduler.
 *
 * Use this variant when the ISR pends a context switch.
 *
 * @note Routed to SystemView when @c OS_TRACE_SEGGER_SYSVIEW and
 *       @c OS_TRACE_ISR are enabled.
 */
void trace_isr_exit_to_scheduler(void);

/* --------------------------------------------------------------------------
 * Delay events
 * -------------------------------------------------------------------------- */
void trace_task_delay_busy_start(TCB_sctTCB_t *task, uint32_t delay_ticks);
void trace_task_delay_busy_end(TCB_sctTCB_t *task);

/* --------------------------------------------------------------------------
 * Generic log event
 * -------------------------------------------------------------------------- */

/**
 * @brief Write a generic null-terminated trace message.
 *
 * @param text Message to emit. A null pointer is ignored.
 *
 * SystemView receives the string directly. The RTT text backend appends one
 * newline after the supplied string.
 *
 * @warning The caller must keep @p text valid for the duration of the call.
 */
void trace_log(const char *text);

#else

static inline void trace_init(void) {}

/* --------------------------------------------------------------------------
 * Task events
 * -------------------------------------------------------------------------- */

static inline void trace_task_create(TCB_sctTCB_t *task) {}
static inline void trace_task_state(uint8_t task_id, uint8_t old_state, uint8_t new_state) {}

/* --------------------------------------------------------------------------
 * Scheduler events
 * -------------------------------------------------------------------------- */

static inline void trace_task_ready(TCB_sctTCB_t *task) {}
static inline void trace_task_run(TCB_sctTCB_t *task) {}
static inline void trace_task_stop_run(void) {}
static inline void trace_task_block(TCB_sctTCB_t *task) {}
static inline void trace_idle(void) {}
static inline void trace_tick(uint32_t dt) {}

/* --------------------------------------------------------------------------
 * ISR events
 * -------------------------------------------------------------------------- */

static inline void trace_isr_enter(void) {}
static inline void trace_isr_exit(void) {}
static inline void trace_isr_exit_to_scheduler(void) {}

/* --------------------------------------------------------------------------
 * Delay events
 * -------------------------------------------------------------------------- */
static inline void trace_task_delay_busy_start(TCB_sctTCB_t *task) {}
static inline void trace_task_delay_busy_end(TCB_sctTCB_t *task) {}

/* --------------------------------------------------------------------------
 * Generic log event
 * -------------------------------------------------------------------------- */

static inline void trace_log(const char *text) {}

#endif /* OS_TRACE_ENABLED */