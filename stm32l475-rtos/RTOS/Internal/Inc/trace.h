/**
 * @file trace.h
 * @brief Backend-independent kernel trace-event interface.
 * @author Jerome
 *
 * @details
 * This interface is the only trace API used by the RTOS kernel. It deliberately
 * does not include or expose the task-control block, scheduler internals, or
 * synchronization-object implementation types. Kernel code translates its
 * internal task representation into the small value types declared here before
 * emitting an event.
 *
 * The trace implementation can route events to SEGGER SystemView and/or to the
 * compact TeSSLa binary stream over SEGGER RTT. SystemView task metadata is
 * cached inside the trace subsystem when a task is registered. This allows the
 * SystemView OS callback to resend the complete task list when recording starts
 * later, without querying the kernel and without introducing a Trace -> Kernel
 * dependency.
 *
 * Normal tasks are registered with SystemView as tasks. The idle task is kept
 * in the trace registry for TeSSLa task/state verification but is intentionally
 * not registered as a normal SystemView task; idle execution is represented by
 * SEGGER_SYSVIEW_OnIdle().
 *
 * When @c OS_TRACE_ENABLED is false, this header provides type-compatible
 * inline no-op functions. Kernel call sites therefore need no conditional
 * compilation and no trace backend is referenced by the kernel.
 *
 * @warning Trace functions can be called from critical sections and interrupt
 *          context. Enabled backends must therefore remain non-blocking and
 *          must never call RTOS services that can block or schedule.
 */

#pragma once

#include "os_config.h"

#include <stdint.h>

/** @brief Reserved task ID used when no task is associated with an event. */
#define TRACE_TASK_ID_NONE UINT8_MAX

/**
 * @brief Classification of a registered RTOS task.
 */
typedef enum {
    TRACE_TASK_KIND_NORMAL = 0, /**< Normal schedulable application task. */
    TRACE_TASK_KIND_IDLE        /**< Kernel idle task; represented by OnIdle() in SystemView. */
} trace_task_kind_t;

/**
 * @brief Minimal task identity used by ordinary trace events.
 *
 * This value contains only information that is meaningful to trace consumers.
 * It intentionally contains no pointer to a TCB or other kernel-owned object.
 */
typedef struct {
    uint8_t id;       /**< Stable RTOS task ID, or @ref TRACE_TASK_ID_NONE. */
    uint8_t priority; /**< RTOS scheduling priority; ignored when @ref id is NONE. */
} trace_task_ref_t;

/**
 * @brief Task metadata cached by the trace subsystem at creation time.
 *
 * @details
 * @ref runtime_id is an opaque, stable runtime identity. On this Cortex-M
 * implementation the kernel supplies the TCB address so SystemView can use a
 * RAM-based task identifier compatible with @c SEGGER_SYSVIEW_SetRAMBase().
 * The trace subsystem stores the numeric task ID separately and all later
 * kernel events refer to the task only by @ref trace_task_ref_t.
 */
typedef struct {
    trace_task_ref_t task;  /**< Numeric RTOS identity and priority. */
    uintptr_t runtime_id;   /**< Opaque stable runtime identity used by trace backends. */
    uintptr_t stack_base;   /**< Lowest address of the task's stack allocation. */
    uint32_t stack_size;    /**< Stack allocation size in bytes. */
    trace_task_kind_t kind; /**< Normal or idle task classification. */
} trace_task_info_t;

/**
 * @brief Construct a normal trace task reference.
 *
 * @param id Stable RTOS task ID.
 * @param priority Current fixed RTOS task priority.
 * @return Initialized trace task reference.
 */
static inline trace_task_ref_t trace_task_ref(uint8_t id, uint8_t priority) {
    trace_task_ref_t ref = { .id = id, .priority = priority };
    return ref;
}

/**
 * @brief Construct the sentinel reference used when no task is associated.
 *
 * @return Task reference with @ref TRACE_TASK_ID_NONE.
 */
static inline trace_task_ref_t trace_task_ref_none(void) {
    trace_task_ref_t ref = { .id = TRACE_TASK_ID_NONE, .priority = 0u };
    return ref;
}

#if OS_TRACE_ENABLED

/**
 * @brief Initialize every enabled trace backend.
 *
 * The RTT backend is initialized and emits the binary session-start record.
 * SystemView is configured and receives a startup message.
 *
 * @pre Call once during @c os_init() before task creation and scheduler start.
 */
void trace_init(void);

/* --------------------------------------------------------------------------
 * Task events
 * -------------------------------------------------------------------------- */

/**
 * @brief Register a newly created task and cache its metadata.
 *
 * @param info Complete trace metadata for the new task.
 *
 * @pre @p info must describe a unique valid RTOS task ID.
 * @pre @ref trace_init must have been called first.
 *
 * @note Normal tasks are announced to SystemView immediately and can later be
 *       replayed by its task-list callback. Idle is cached but not registered
 *       as a normal SystemView task.
 * @note TeSSLa receives @c TASK_CREATE for both normal and idle tasks so its
 *       task-state model remains complete.
 */
void trace_task_register(const trace_task_info_t *info);

/**
 * @brief Record a task-state transition.
 *
 * @param task_id Numeric ID of the affected task.
 * @param old_state Previous task-state value.
 * @param new_state New task-state value.
 *
 * @note The current implementation emits this event only through the RTT binary
 *       backend when @c OS_TRACE_TASKS is enabled.
 */
void trace_task_state(uint8_t task_id, uint8_t old_state, uint8_t new_state);

/* --------------------------------------------------------------------------
 * Scheduler events
 * -------------------------------------------------------------------------- */

/**
 * @brief Record that a task entered the ready state.
 *
 * @param task Trace reference of the task entering the ready state.
 *
 * @note Emitted only when @c OS_TRACE_SCHEDULER is enabled.
 */
void trace_task_ready(trace_task_ref_t task);

/**
 * @brief Record that the scheduler started executing a task.
 *
 * @param task Trace reference of the task entering the running state.
 *
 * @note Emitted only when @c OS_TRACE_SCHEDULER is enabled.
 */
void trace_task_run(trace_task_ref_t task);

/**
 * @brief Record that execution of the current task stopped.
 *
 * @note Emitted only when @c OS_TRACE_SCHEDULER is enabled.
 */
void trace_task_stop_run(void);

/**
 * @brief Record that a task entered the blocked state.
 *
 * @param task Trace reference of the task entering the blocked state.
 *
 * @note Emitted only when @c OS_TRACE_SCHEDULER is enabled.
 */
void trace_task_block(trace_task_ref_t task);

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
 * @note The current implementation emits this event through the RTT binary
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

/**
 * @brief Record the start of a busy-wait delay.
 *
 * Marks the point at which @p task begins actively polling the kernel tick for
 * @p delay_ticks ticks. The task remains runnable during this interval; the
 * event does not represent a scheduler block or yield.
 *
 * @param task Trace reference of the task beginning the busy wait.
 * @param delay_ticks Requested busy-wait duration in kernel ticks.
 *
 * @pre @p delay_ticks must satisfy the validation performed by
 *      @c os_delay_busy().
 *
 * @note Emits @c DELAY_BUSY_START with the task ID and requested tick count.
 * @note Emitted when @c OS_TRACE_DELAY is enabled and routed to each
 *       configured backend that supports this event.
 * @note Pair this event with one later call to
 *       @c trace_task_delay_busy_end() for the same task.
 */
void trace_task_delay_busy_start(trace_task_ref_t task, uint32_t delay_ticks);

/**
 * @brief Record completion of a busy-wait delay.
 *
 * Marks the point at which @p task has observed the requested busy-wait
 * interval elapse and is about to return from @c os_delay_busy().
 *
 * @param task Trace reference of the task completing the busy wait.
 *
 * @pre A matching @c trace_task_delay_busy_start() event must already have
 *      been emitted for @p task.
 *
 * @note Emits @c DELAY_BUSY_END with the task ID.
 * @note Emitted when @c OS_TRACE_DELAY is enabled and routed to each
 *       configured backend that supports this event.
 */
void trace_task_delay_busy_end(trace_task_ref_t task);

/**
 * @brief Record the start of a scheduler-based blocking delay.
 *
 * Marks a successful nonzero @c os_delay() operation immediately before the
 * calling task is blocked for @p delay_ticks ticks. This event brackets the
 * scheduler wait; it is distinct from the CPU-consuming busy-delay events.
 *
 * @param task Trace reference of the task entering the delay wait.
 * @param delay_ticks Requested blocking duration in kernel ticks.
 *
 * @pre @p delay_ticks must be nonzero and must satisfy the finite-timeout
 *      validation performed by @c os_delay().
 *
 * @note Emits @c DELAY_START with the task ID and requested tick count.
 * @note Emitted when @c OS_TRACE_DELAY is enabled and routed to each
 *       configured backend that supports this event.
 * @note A zero-tick @c os_delay(0) is a yield and must not emit this event.
 * @note Pair this event with one later call to @c trace_task_delay_end() for
 *       the same task after its delay expires.
 */
void trace_task_delay_start(trace_task_ref_t task, uint32_t delay_ticks);

/**
 * @brief Record completion of a scheduler-based blocking delay.
 *
 * Marks the point at which @p task resumes after its delay timeout has
 * expired. It must not be emitted for a rejected delay request or a zero-tick
 * yield.
 *
 * @param task Trace reference of the task whose blocking delay completed.
 *
 * @pre A matching @c trace_task_delay_start() event must already have been
 *      emitted for @p task.
 *
 * @note Emits @c DELAY_END with the task ID.
 * @note Emitted when @c OS_TRACE_DELAY is enabled and routed to each
 *       configured backend that supports this event.
 */
void trace_task_delay_end(trace_task_ref_t task);

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
 * @param task Trace reference of the task attempting the acquire, or
 *             @ref trace_task_ref_none when no task is associated with the
 *             operation (for example, exception context or pre-scheduler use).
 * @param count Number of available tokens observed before the attempt.
 * @param timeout_ticks Requested timeout in kernel ticks.
 * @param finite_timeout Nonzero if @p timeout_ticks is a finite deadline;
 *                       zero for a non-timed/block-forever operation.
 *
 * @note @ref trace_task_ref_none is encoded as task ID @c UINT8_MAX in the RTT event.
 */
void trace_sem_acquire_enter(const void *semaphore,
                             trace_task_ref_t task,
                             uint32_t count,
                             uint32_t timeout_ticks,
                             uint8_t finite_timeout);

/**
 * @brief Record completion of an acquire operation.
 *
 * @param semaphore Stable address of the semaphore object.
 * @param task Trace reference of the task completing the acquire, or
 *             @ref trace_task_ref_none when no task is associated with the operation.
 * @param count Number of available tokens after completion.
 * @param succeeded Nonzero only when one token was acquired.
 *
 * Emit this event on every normal return, including non-blocking failure and
 * timeout. A blocked acquire that is later released emits it only after the
 * task resumes and the acquire actually completes.
 * @ref trace_task_ref_none is encoded as task ID @c UINT8_MAX in the RTT event.
 */
void trace_sem_acquire_exit(const void *semaphore,
                            trace_task_ref_t task,
                            uint32_t count,
                            uint8_t succeeded);

/**
 * @brief Record that an acquire operation queued and blocked its task.
 */
void trace_sem_block(const void *semaphore,
                     trace_task_ref_t task,
                     uint32_t timeout_ticks,
                     uint8_t finite_timeout);

/**
 * @brief Record expiry of a finite semaphore-acquire timeout.
 */
void trace_sem_timeout(const void *semaphore, trace_task_ref_t task, uint32_t count);

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
 * The task priority is taken from the trace reference so the verifier can check that the
 * highest-priority waiter was selected. Trace sequence order breaks ties.
 */
void trace_sem_wake(const void *semaphore, trace_task_ref_t task);

/* --------------------------------------------------------------------------
 * Mutex events
 * -------------------------------------------------------------------------- */

/**
 * @brief Record initialization of an unlocked, non-recursive mutex.
 *
 * @param mutex Stable address of the initialized mutex object.
 *
 * @pre @p mutex must not be null.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_create(const void *mutex);

/**
 * @brief Record the start of a mutex-lock operation.
 *
 * @param mutex Stable address of the mutex object.
 * @param task Trace reference of the task attempting to lock the mutex.
 * @param owner Current mutex owner, or @ref trace_task_ref_none if the mutex is unlocked.
 * @param timeout_ticks Requested timeout in kernel ticks.
 * @param finite_timeout Nonzero if @p timeout_ticks represents a finite
 *        deadline; zero for a non-timed or wait-forever operation.
 *
 * @pre @p mutex must not be null.
 * @note @ref trace_task_ref_none is encoded as task ID @c UINT8_MAX.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_lock_enter(const void *mutex,
                            trace_task_ref_t task,
                            trace_task_ref_t owner,
                            uint32_t timeout_ticks,
                            uint8_t finite_timeout);

/**
 * @brief Record completion of a mutex-lock operation.
 *
 * @param mutex Stable address of the mutex object.
 * @param task Trace reference of the task that attempted to lock the mutex.
 * @param owner Mutex owner after the operation, or @ref trace_task_ref_none if it is unlocked.
 * @param succeeded Nonzero if ownership was acquired; zero otherwise.
 *
 * @pre @p mutex must not be null.
 * @note @ref trace_task_ref_none is encoded as task ID @c UINT8_MAX.
 * @note A blocked operation emits this event after the task resumes.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_lock_exit(const void *mutex,
                           trace_task_ref_t task,
                           trace_task_ref_t owner,
                           uint8_t succeeded);

/**
 * @brief Record that a mutex-lock operation queued and blocked its task.
 *
 * @param mutex Stable address of the mutex object.
 * @param task Trace reference of the task added to the mutex wait queue.
 * @param owner Trace reference of the task owning the mutex when the caller was blocked.
 * @param timeout_ticks Requested timeout in kernel ticks.
 * @param finite_timeout Nonzero if @p timeout_ticks represents a finite
 *        deadline; zero for a wait-forever operation.
 *
 * @pre @p mutex must not be null.
 * @note The task priority is carried by @p task.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_block(const void *mutex,
                       trace_task_ref_t task,
                       trace_task_ref_t owner,
                       uint32_t timeout_ticks,
                       uint8_t finite_timeout);

/**
 * @brief Record expiry of a finite mutex-lock timeout.
 *
 * @param mutex Stable address of the mutex object.
 * @param task Trace reference of the task whose lock operation timed out.
 * @param owner Current mutex owner, or @ref trace_task_ref_none if the mutex is unlocked.
 *
 * @pre @p mutex must not be null.
 * @note @ref trace_task_ref_none is encoded as task ID @c UINT8_MAX.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_timeout(const void *mutex, trace_task_ref_t task, trace_task_ref_t owner);

/**
 * @brief Record the result of a mutex-unlock operation.
 *
 * @param mutex Stable address of the mutex object.
 * @param task Trace reference of the task attempting to unlock the mutex.
 * @param owner_before Mutex owner before the operation, or @ref trace_task_ref_none if unowned.
 * @param owner_after Mutex owner after the operation, or @ref trace_task_ref_none if unowned.
 * @param succeeded Nonzero if the unlock or ownership handoff succeeded;
 *        zero if the operation was rejected.
 *
 * @pre @p mutex must not be null.
 * @note @ref trace_task_ref_none values are encoded as task ID @c UINT8_MAX.
 * @note During direct handoff, @p owner_after identifies the selected waiter.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_unlock(const void *mutex,
                        trace_task_ref_t task,
                        trace_task_ref_t owner_before,
                        trace_task_ref_t owner_after,
                        uint8_t succeeded);

/**
 * @brief Record the waiter selected for direct mutex ownership handoff.
 *
 * @param mutex Stable address of the mutex object.
 * @param task Trace reference of the waiting task selected as the new mutex owner.
 *
 * @pre @p mutex must not be null.
 * @note The task priority is carried by @p task so the verifier can check
 *       priority ordering. Trace order is used to resolve FIFO ties.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_wake(const void *mutex, trace_task_ref_t task);

/* --------------------------------------------------------------------------
 * Message-queue events
 * -------------------------------------------------------------------------- */

/**
 * @brief Record creation of a message queue.
 *
 * @param queue_id Stable numeric identifier of the queue.
 * @param capacity Maximum number of buffered messages held by the queue.
 *
 * The identifier and capacity must match the queue configuration used to
 * generate the TeSSLa monitor.
 *
 * @note Emitted when @c OS_TRACE_QUEUE is enabled and routed to each
 *       configured backend that supports this event.
 */
void trace_queue_create(uint32_t queue_id, uint32_t capacity);

/**
 * @brief Record the start of a queue-send operation.
 *
 * @param queue_id Identifier of the target queue.
 * @param task_id Identifier of the sending task, or @c UINT8_MAX for an
 *                operation performed from exception context.
 * @param task_priority Priority of the sending task.
 * @param timeout_ticks Requested timeout in kernel ticks. @c OS_NO_WAIT and
 *                      @c OS_WAIT_FOREVER retain their public API meanings.
 * @param message_hash 32-bit hash of the message being sent.
 *
 * Emit before determining whether the message is buffered, handed directly to
 * a receiver, rejected, or must block.
 *
 * @note Emitted when @c OS_TRACE_QUEUE is enabled and routed to each
 *       configured backend that supports this event.
 */
void trace_queue_send_attempt(uint32_t queue_id,
                              uint8_t task_id,
                              uint8_t task_priority,
                              uint32_t timeout_ticks,
                              uint32_t message_hash);

/**
 * @brief Record successful completion of a queue-send operation.
 *
 * @param queue_id Identifier of the target queue.
 * @param task_id Identifier of the sender, or @c UINT8_MAX for exception
 *                context.
 * @param message_hash Hash supplied by the corresponding send attempt.
 *
 * A successful send either places the message in the ring buffer or transfers
 * it directly to a waiting receiver.
 */
void trace_queue_send_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash);

/**
 * @brief Record that a queue-send operation blocked its task.
 *
 * @param queue_id Identifier of the full queue.
 * @param task_id Identifier of the task entering the send wait queue.
 * @param task_priority Priority used to order the waiting sender.
 *
 * @pre The task must subsequently transition from @c RUNNING to @c BLOCKED.
 */
void trace_queue_send_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority);

/**
 * @brief Record expiry of a blocked queue-send operation.
 *
 * @param queue_id Identifier of the queue on which the task waited.
 * @param task_id Identifier of the timed-out sender.
 *
 * @note Emit only for a finite timeout after the requested number of kernel
 *       ticks has elapsed.
 */
void trace_queue_send_timeout(uint32_t queue_id, uint8_t task_id);

/**
 * @brief Record the start of a queue-receive operation.
 *
 * @param queue_id Identifier of the source queue.
 * @param task_id Identifier of the receiving task, or @c UINT8_MAX for an
 *                operation performed from exception context.
 * @param task_priority Priority of the receiving task.
 * @param timeout_ticks Requested timeout in kernel ticks. @c OS_NO_WAIT and
 *                      @c OS_WAIT_FOREVER retain their public API meanings.
 *
 * Emit before determining whether a buffered message or waiting sender can
 * satisfy the operation, or whether the operation must fail or block.
 */
void trace_queue_receive_attempt(uint32_t queue_id,
                                 uint8_t task_id,
                                 uint8_t task_priority,
                                 uint32_t timeout_ticks);

/**
 * @brief Record successful completion of a queue-receive operation.
 *
 * @param queue_id Identifier of the source queue.
 * @param task_id Identifier of the receiver, or @c UINT8_MAX for exception
 *                context.
 * @param message_hash 32-bit hash of the received message.
 *
 * The hash must describe the bytes delivered to the receiver, whether the
 * message came from the ring buffer or through direct handoff.
 */
void trace_queue_receive_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash);

/**
 * @brief Record that a queue-receive operation blocked its task.
 *
 * @param queue_id Identifier of the empty queue.
 * @param task_id Identifier of the task entering the receive wait queue.
 * @param task_priority Priority used to order the waiting receiver.
 *
 * @pre The task must subsequently transition from @c RUNNING to @c BLOCKED.
 */
void trace_queue_receive_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority);

/**
 * @brief Record expiry of a blocked queue-receive operation.
 *
 * @param queue_id Identifier of the queue on which the task waited.
 * @param task_id Identifier of the timed-out receiver.
 *
 * @note Emit only for a finite timeout after the requested number of kernel
 *       ticks has elapsed.
 */
void trace_queue_receive_timeout(uint32_t queue_id, uint8_t task_id);

/**
 * @brief Record that a waiting sender was selected for wakeup.
 *
 * @param queue_id Identifier of the queue whose buffered receive freed a slot.
 * @param task_id Identifier of the sender leaving the send wait queue.
 *
 * @pre The task must subsequently transition from @c BLOCKED to @c READY.
 */
void trace_queue_wake_sender(uint32_t queue_id, uint8_t task_id);

/**
 * @brief Record that a waiting receiver was selected for wakeup.
 *
 * @param queue_id Identifier of the queue receiving a direct handoff.
 * @param task_id Identifier of the receiver leaving the receive wait queue.
 *
 * @pre The task must subsequently transition from @c BLOCKED to @c READY.
 */
void trace_queue_wake_receiver(uint32_t queue_id, uint8_t task_id);

/**
 * @brief Record a direct message transfer between a sender and receiver.
 *
 * @param queue_id Identifier of the queue coordinating the transfer.
 * @param sender_id Identifier of the sending task, or @c UINT8_MAX for
 *                  exception context.
 * @param receiver_id Identifier of the receiving task, or @c UINT8_MAX for
 *                    exception context.
 * @param message_hash 32-bit hash of the transferred message.
 *
 * Direct handoff bypasses the ring buffer and therefore does not change queue
 * fill. Emit this event immediately before the matching send-success and
 * receive-success events.
 */
void trace_queue_handoff(uint32_t queue_id,
                         uint8_t sender_id,
                         uint8_t receiver_id,
                         uint32_t message_hash);

/**
 * @brief Record the queue fill level after a ring-buffer modification.
 *
 * @param queue_id Identifier of the modified queue.
 * @param fill Number of messages buffered after the push or pop.
 *
 * Emit immediately after every successful ring-buffer push or pop. Do not emit
 * for direct handoff because it leaves the fill level unchanged.
 */
void trace_queue_fill(uint32_t queue_id, uint32_t fill);

/* --------------------------------------------------------------------------
 * Project: 3D-Gyro-Accelerometer events
 * -------------------------------------------------------------------------- */

void trace_transmission_complete(void);

/* --------------------------------------------------------------------------
 * Generic log event
 * -------------------------------------------------------------------------- */

/**
 * @brief Write a generic null-terminated trace message.
 *
 * @param text Message to emit. A null pointer is ignored.
 *
 * SystemView receives the string directly. The RTT binary backend emits the
 * bounded string bytes as a LOG payload.
 *
 * @warning The caller must keep @p text valid for the duration of the call.
 */
void trace_log(const char *text);

#else /* OS_TRACE_ENABLED */

static inline void trace_init(void) {}

/* --------------------------------------------------------------------------
 * Task events
 * -------------------------------------------------------------------------- */
static inline void trace_task_register(const trace_task_info_t *info) {}
static inline void trace_task_state(uint8_t task_id, uint8_t old_state, uint8_t new_state) {}

/* --------------------------------------------------------------------------
 * Scheduler events
 * -------------------------------------------------------------------------- */
static inline void trace_task_ready(trace_task_ref_t task) {}
static inline void trace_task_run(trace_task_ref_t task) {}
static inline void trace_task_stop_run(void) {}
static inline void trace_task_block(trace_task_ref_t task) {}
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
static inline void trace_task_delay_busy_start(trace_task_ref_t task, uint32_t delay_ticks) {}

static inline void trace_task_delay_busy_end(trace_task_ref_t task) {}

static inline void trace_task_delay_start(trace_task_ref_t task, uint32_t delay_ticks) {}

static inline void trace_task_delay_end(trace_task_ref_t task) {}

/* --------------------------------------------------------------------------
 * Counting-semaphore events
 * -------------------------------------------------------------------------- */
static inline void trace_sem_create(const void *semaphore,
                                    uint32_t initial_count,
                                    uint32_t max_count) {}

static inline void trace_sem_acquire_enter(const void *semaphore,
                                           trace_task_ref_t task,
                                           uint32_t count,
                                           uint32_t timeout_ticks,
                                           uint8_t finite_timeout) {}

static inline void trace_sem_acquire_exit(const void *semaphore,
                                          trace_task_ref_t task,
                                          uint32_t count,
                                          uint8_t succeeded) {}

static inline void trace_sem_block(const void *semaphore,
                                   trace_task_ref_t task,
                                   uint32_t timeout_ticks,
                                   uint8_t finite_timeout) {}

static inline void trace_sem_timeout(const void *semaphore, trace_task_ref_t task, uint32_t count) {
}

static inline void trace_sem_release(const void *semaphore,
                                     uint32_t count_before,
                                     uint32_t count_after,
                                     uint32_t max_count,
                                     uint8_t succeeded) {}

static inline void trace_sem_wake(const void *semaphore, trace_task_ref_t task) {}

/* --------------------------------------------------------------------------
 * Mutex events
 * -------------------------------------------------------------------------- */
static inline void trace_mutex_create(const void *mutex) {}

static inline void trace_mutex_lock_enter(const void *mutex,
                                          trace_task_ref_t task,
                                          trace_task_ref_t owner,
                                          uint32_t timeout_ticks,
                                          uint8_t finite_timeout) {}

static inline void trace_mutex_lock_exit(const void *mutex,
                                         trace_task_ref_t task,
                                         trace_task_ref_t owner,
                                         uint8_t succeeded) {}

static inline void trace_mutex_block(const void *mutex,
                                     trace_task_ref_t task,
                                     trace_task_ref_t owner,
                                     uint32_t timeout_ticks,
                                     uint8_t finite_timeout) {}

static inline void trace_mutex_timeout(const void *mutex,
                                       trace_task_ref_t task,
                                       trace_task_ref_t owner) {}

static inline void trace_mutex_unlock(const void *mutex,
                                      trace_task_ref_t task,
                                      trace_task_ref_t owner_before,
                                      trace_task_ref_t owner_after,
                                      uint8_t succeeded) {}

static inline void trace_mutex_wake(const void *mutex, trace_task_ref_t task) {}

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
 * Project: 3D-Gyro-Accelerometer events
 * -------------------------------------------------------------------------- */
static inline void trace_transmission_complete(void) {}

/* --------------------------------------------------------------------------
 * Generic log event
 * -------------------------------------------------------------------------- */
static inline void trace_log(const char *text) {}

#endif /* OS_TRACE_ENABLED */
