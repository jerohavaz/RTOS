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
 * Counting-semaphore events
 * -------------------------------------------------------------------------- */

/**
 * @brief Record creation of a counting semaphore.
 *
 * @param semaphore Stable address of the semaphore object.
 * @param initial_count Number of initially available tokens.
 * @param max_count Maximum number of tokens the semaphore can hold.
 *
 * @note A binary semaphore is represented by @p max_count equal to one.
 * @note Emitted only when @c OS_TRACE_SEMAPHORE is enabled.
 */
void trace_sem_create(const void *semaphore, uint32_t initial_count, uint32_t max_count);

/**
 * @brief Record the start of an acquire operation.
 *
 * @param semaphore Stable address of the semaphore object.
 * @param task Task attempting the acquire, or null when no task owns the
 *             operation (for example, exception context or pre-scheduler use).
 * @param count Number of available tokens observed before the attempt.
 * @param timeout_ticks Requested timeout in kernel ticks.
 * @param finite_timeout Nonzero if @p timeout_ticks is a finite deadline;
 *                       zero for a non-timed/block-forever operation.
 *
 * @note A null @p task is encoded as task ID @c UINT8_MAX in the RTT event.
 */
void trace_sem_acquire_enter(const void *semaphore,
                             TCB_sctTCB_t *task,
                             uint32_t count,
                             uint32_t timeout_ticks,
                             uint8_t finite_timeout);

/**
 * @brief Record completion of an acquire operation.
 *
 * @param semaphore Stable address of the semaphore object.
 * @param task Task completing the acquire, or null when no task owns the
 *             operation.
 * @param count Number of available tokens after completion.
 * @param succeeded Nonzero only when one token was acquired.
 *
 * Emit this event on every normal return, including non-blocking failure and
 * timeout. A blocked acquire that is later released emits it only after the
 * task resumes and the acquire actually completes.
 * A null @p task is encoded as task ID @c UINT8_MAX in the RTT event.
 */
void trace_sem_acquire_exit(const void *semaphore,
                            TCB_sctTCB_t *task,
                            uint32_t count,
                            uint8_t succeeded);

/**
 * @brief Record that an acquire operation queued and blocked its task.
 */
void trace_sem_block(const void *semaphore,
                     TCB_sctTCB_t *task,
                     uint32_t timeout_ticks,
                     uint8_t finite_timeout);

/**
 * @brief Record expiry of a finite semaphore-acquire timeout.
 */
void trace_sem_timeout(const void *semaphore, TCB_sctTCB_t *task, uint32_t count);

/**
 * @brief Record the result of a release operation.
 *
 * @param count_before Available-token count before the release.
 * @param count_after Available-token count after the release or direct
 *                    hand-off to a waiter.
 * @param max_count Configured semaphore capacity.
 * @param succeeded Nonzero if the release was accepted.
 *
 * @note Direct hand-off is allowed to leave the count unchanged when a waiter
 *       receives the released token.
 */
void trace_sem_release(const void *semaphore,
                       uint32_t count_before,
                       uint32_t count_after,
                       uint32_t max_count,
                       uint8_t succeeded);

/**
 * @brief Record that release selected a waiting task for wakeup.
 *
 * The task priority is taken from the TCB so the verifier can check that the
 * highest-priority waiter was selected. Trace sequence order breaks ties.
 */
void trace_sem_wake(const void *semaphore, TCB_sctTCB_t *task);

/* --------------------------------------------------------------------------
 * Message queue events
 * -------------------------------------------------------------------------- */

void trace_queue_create(uint32_t queue_id, uint32_t capacity);

void trace_queue_send_attempt(uint32_t queue_id,
                              uint8_t task_id,
                              uint8_t task_priority,
                              uint32_t timeout_ticks,
                              uint32_t message_hash);

void trace_queue_send_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash);

void trace_queue_send_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority);

void trace_queue_send_timeout(uint32_t queue_id, uint8_t task_id);

void trace_queue_receive_attempt(uint32_t queue_id,
                                 uint8_t task_id,
                                 uint8_t task_priority,
                                 uint32_t timeout_ticks);

void trace_queue_receive_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash);

void trace_queue_receive_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority);

void trace_queue_receive_timeout(uint32_t queue_id, uint8_t task_id);

void trace_queue_wake_sender(uint32_t queue_id, uint8_t task_id);

void trace_queue_wake_receiver(uint32_t queue_id, uint8_t task_id);

void trace_queue_handoff(uint32_t queue_id,
                         uint8_t sender_id,
                         uint8_t receiver_id,
                         uint32_t message_hash);

void trace_queue_fill(uint32_t queue_id, uint32_t fill);

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
static inline void trace_task_delay_busy_start(TCB_sctTCB_t *task, uint32_t delay_ticks) {}
static inline void trace_task_delay_busy_end(TCB_sctTCB_t *task) {}

/* --------------------------------------------------------------------------
 * Counting-semaphore events
 * -------------------------------------------------------------------------- */
static inline void trace_sem_create(const void *semaphore,
                                    uint32_t initial_count,
                                    uint32_t max_count) {}
static inline void trace_sem_acquire_enter(const void *semaphore,
                                           TCB_sctTCB_t *task,
                                           uint32_t count,
                                           uint32_t timeout_ticks,
                                           uint8_t finite_timeout) {}
static inline void trace_sem_acquire_exit(const void *semaphore,
                                          TCB_sctTCB_t *task,
                                          uint32_t count,
                                          uint8_t succeeded) {}
static inline void trace_sem_block(const void *semaphore,
                                   TCB_sctTCB_t *task,
                                   uint32_t timeout_ticks,
                                   uint8_t finite_timeout) {}
static inline void trace_sem_timeout(const void *semaphore, TCB_sctTCB_t *task, uint32_t count) {}
static inline void trace_sem_release(const void *semaphore,
                                     uint32_t count_before,
                                     uint32_t count_after,
                                     uint32_t max_count,
                                     uint8_t succeeded) {}
static inline void trace_sem_wake(const void *semaphore, TCB_sctTCB_t *task) {}

/* --------------------------------------------------------------------------
 * Message queue events
 * -------------------------------------------------------------------------- */

static inline void trace_queue_create(uint32_t queue_id, uint32_t capacity) {}

static inline void trace_queue_send_attempt(uint32_t queue_id,
                                            uint8_t task_id,
                                            uint8_t task_priority,
                                            uint32_t timeout_ticks,
                                            uint32_t message_hash) {}

static inline void trace_queue_send_success(uint32_t queue_id,
                                            uint8_t task_id,
                                            uint32_t message_hash) {}

static inline void trace_queue_send_block(uint32_t queue_id,
                                          uint8_t task_id,
                                          uint8_t task_priority) {}

static inline void trace_queue_send_timeout(uint32_t queue_id, uint8_t task_id) {}

static inline void trace_queue_receive_attempt(uint32_t queue_id,
                                               uint8_t task_id,
                                               uint8_t task_priority,
                                               uint32_t timeout_ticks) {}

static inline void trace_queue_receive_success(uint32_t queue_id,
                                               uint8_t task_id,
                                               uint32_t message_hash) {}

static inline void trace_queue_receive_block(uint32_t queue_id,
                                             uint8_t task_id,
                                             uint8_t task_priority) {}

static inline void trace_queue_receive_timeout(uint32_t queue_id, uint8_t task_id) {}

static inline void trace_queue_wake_sender(uint32_t queue_id, uint8_t task_id) {}

static inline void trace_queue_wake_receiver(uint32_t queue_id, uint8_t task_id) {}

static inline void trace_queue_handoff(uint32_t queue_id,
                                       uint8_t sender_id,
                                       uint8_t receiver_id,
                                       uint32_t message_hash) {}

static inline void trace_queue_fill(uint32_t queue_id, uint32_t fill) {}

/* --------------------------------------------------------------------------
 * Generic log event
 * -------------------------------------------------------------------------- */

static inline void trace_log(const char *text) {}

#endif /* OS_TRACE_ENABLED */
