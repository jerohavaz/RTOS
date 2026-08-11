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
 * TeSSLa-compatible text stream over SEGGER RTT.
 *
 * SystemView uses its native task, scheduler, idle, and ISR events wherever
 * possible. Delay, semaphore, mutex, and message-queue operations are recorded
 * as compact custom OS-API events with fixed IDs in the SystemView user-event
 * range 32..511. Human-readable names and parameter formats are kept host-side
 * in @c SYSVIEW_CustomRTOS.txt, so no description callback runs on the target
 * during timing measurements.
 *
 * Explicit task-state and kernel-tick records are emitted only to the TeSSLa
 * RTT stream. SystemView already represents task state through its native task
 * events and SysTick timing through native ISR timestamps, so mirroring those
 * records would only duplicate information and increase trace traffic.
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
 * configured and started. Custom delay/semaphore/mutex/queue events use fixed
 * OS-API event IDs and are decoded by the host-side
 * @c SYSVIEW_PE5001_RTOS.txt description file.
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
 * @note Emitted to the TeSSLa RTT backend when @c OS_TRACE_TESSLA_RTT and
 *       @c OS_TRACE_TASKS are enabled.
 * @note SystemView does not receive a duplicate custom state event; its native
 *       task Ready/Run/Block events provide the corresponding visualization.
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
 * @note Emitted to the TeSSLa RTT backend when @c OS_TRACE_TESSLA_RTT and
 *       @c OS_TRACE_SCHEDULER are enabled.
 * @note SystemView does not receive a custom tick event; SysTick timing is
 *       already visible through native ISR entry/exit events.
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
 * @param task Task beginning the busy wait.
 * @param delay_ticks Requested busy-wait duration in kernel ticks.
 *
 * @pre @p task must not be null.
 * @pre @p delay_ticks must satisfy the validation performed by
 *      @c os_delay_busy().
 *
 * @note Emits @c DELAY_BUSY_START with the task ID and requested tick count.
 * @note Mirrored to every enabled trace backend when @c OS_TRACE_DELAY is
 *       enabled. SystemView records the corresponding structured OS-API event.
 * @note Pair this event with one later call to
 *       @c trace_task_delay_busy_end() for the same task.
 */
void trace_task_delay_busy_start(TCB_sctTCB_t *task, uint32_t delay_ticks);

/**
 * @brief Record completion of a busy-wait delay.
 *
 * Marks the point at which @p task has observed the requested busy-wait
 * interval elapse and is about to return from @c os_delay_busy().
 *
 * @param task Task completing the busy wait.
 *
 * @pre @p task must not be null.
 * @pre A matching @c trace_task_delay_busy_start() event must already have
 *      been emitted for @p task.
 *
 * @note Emits @c DELAY_BUSY_END with the task ID.
 * @note Mirrored to every enabled trace backend when @c OS_TRACE_DELAY is
 *       enabled. SystemView records the corresponding structured OS-API event.
 */
void trace_task_delay_busy_end(TCB_sctTCB_t *task);

/**
 * @brief Record the start of a scheduler-based blocking delay.
 *
 * Marks a successful nonzero @c os_delay() operation immediately before the
 * calling task is blocked for @p delay_ticks ticks. This event brackets the
 * scheduler wait; it is distinct from the CPU-consuming busy-delay events.
 *
 * @param task Task entering the delay wait.
 * @param delay_ticks Requested blocking duration in kernel ticks.
 *
 * @pre @p task must not be null.
 * @pre @p delay_ticks must be nonzero and must satisfy the finite-timeout
 *      validation performed by @c os_delay().
 *
 * @note Emits @c DELAY_START with the task ID and requested tick count.
 * @note Mirrored to every enabled trace backend when @c OS_TRACE_DELAY is
 *       enabled. SystemView records the corresponding structured OS-API event.
 * @note A zero-tick @c os_delay(0) is a yield and must not emit this event.
 * @note Pair this event with one later call to @c trace_task_delay_end() for
 *       the same task after its delay expires.
 */
void trace_task_delay_start(TCB_sctTCB_t *task, uint32_t delay_ticks);

/**
 * @brief Record completion of a scheduler-based blocking delay.
 *
 * Marks the point at which @p task resumes after its delay timeout has
 * expired. It must not be emitted for a rejected delay request or a zero-tick
 * yield.
 *
 * @param task Task whose blocking delay completed.
 *
 * @pre @p task must not be null.
 * @pre A matching @c trace_task_delay_start() event must already have been
 *      emitted for @p task.
 *
 * @note Emits @c DELAY_END with the task ID.
 * @note Mirrored to every enabled trace backend when @c OS_TRACE_DELAY is
 *       enabled. SystemView records the corresponding structured OS-API event.
 */
void trace_task_delay_end(TCB_sctTCB_t *task);

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
 * @note A null @p task is encoded as task ID @c UINT8_MAX in each enabled backend event.
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
 * A null @p task is encoded as task ID @c UINT8_MAX in each enabled backend event.
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
 * @param task Task attempting to lock the mutex.
 * @param owner Current mutex owner, or null if the mutex is unlocked.
 * @param timeout_ticks Requested timeout in kernel ticks.
 * @param finite_timeout Nonzero if @p timeout_ticks represents a finite
 *        deadline; zero for a non-timed or wait-forever operation.
 *
 * @pre @p mutex must not be null.
 * @note A null task or owner is encoded as task ID @c UINT8_MAX.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_lock_enter(const void *mutex,
                            TCB_sctTCB_t *task,
                            TCB_sctTCB_t *owner,
                            uint32_t timeout_ticks,
                            uint8_t finite_timeout);

/**
 * @brief Record completion of a mutex-lock operation.
 *
 * @param mutex Stable address of the mutex object.
 * @param task Task that attempted to lock the mutex.
 * @param owner Mutex owner after the operation, or null if it is unlocked.
 * @param succeeded Nonzero if ownership was acquired; zero otherwise.
 *
 * @pre @p mutex must not be null.
 * @note A null task or owner is encoded as task ID @c UINT8_MAX.
 * @note A blocked operation emits this event after the task resumes.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_lock_exit(const void *mutex,
                           TCB_sctTCB_t *task,
                           TCB_sctTCB_t *owner,
                           uint8_t succeeded);

/**
 * @brief Record that a mutex-lock operation queued and blocked its task.
 *
 * @param mutex Stable address of the mutex object.
 * @param task Task added to the mutex wait queue.
 * @param owner Task owning the mutex when the caller was blocked.
 * @param timeout_ticks Requested timeout in kernel ticks.
 * @param finite_timeout Nonzero if @p timeout_ticks represents a finite
 *        deadline; zero for a wait-forever operation.
 *
 * @pre @p mutex must not be null.
 * @pre @p task must not be null.
 * @pre @p owner must not be null.
 * @note The task priority is obtained from @p task.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_block(const void *mutex,
                       TCB_sctTCB_t *task,
                       TCB_sctTCB_t *owner,
                       uint32_t timeout_ticks,
                       uint8_t finite_timeout);

/**
 * @brief Record expiry of a finite mutex-lock timeout.
 *
 * @param mutex Stable address of the mutex object.
 * @param task Task whose lock operation timed out.
 * @param owner Current mutex owner, or null if the mutex is unlocked.
 *
 * @pre @p mutex must not be null.
 * @pre @p task must not be null.
 * @note A null owner is encoded as task ID @c UINT8_MAX.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_timeout(const void *mutex, TCB_sctTCB_t *task, TCB_sctTCB_t *owner);

/**
 * @brief Record the result of a mutex-unlock operation.
 *
 * @param mutex Stable address of the mutex object.
 * @param task Task attempting to unlock the mutex.
 * @param owner_before Mutex owner before the operation, or null if unowned.
 * @param owner_after Mutex owner after the operation, or null if unowned.
 * @param succeeded Nonzero if the unlock or ownership handoff succeeded;
 *        zero if the operation was rejected.
 *
 * @pre @p mutex must not be null.
 * @note Null task or owner values are encoded as task ID @c UINT8_MAX.
 * @note During direct handoff, @p owner_after identifies the selected waiter.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_unlock(const void *mutex,
                        TCB_sctTCB_t *task,
                        TCB_sctTCB_t *owner_before,
                        TCB_sctTCB_t *owner_after,
                        uint8_t succeeded);

/**
 * @brief Record the waiter selected for direct mutex ownership handoff.
 *
 * @param mutex Stable address of the mutex object.
 * @param task Waiting task selected as the new mutex owner.
 *
 * @pre @p mutex must not be null.
 * @pre @p task must not be null.
 * @note The task priority is obtained from @p task so the verifier can check
 *       priority ordering. Trace order is used to resolve FIFO ties.
 * @note Emitted only when @c OS_TRACE_MUTEX is enabled.
 */
void trace_mutex_wake(const void *mutex, TCB_sctTCB_t *task);

/* --------------------------------------------------------------------------
 * Message queue events
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
 * @note Mirrored to every enabled trace backend when @c OS_TRACE_QUEUE is
 *       enabled. SystemView records the same numeric fields as a structured
 *       user event.
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
 * @note Mirrored to every enabled trace backend when @c OS_TRACE_QUEUE is
 *       enabled. SystemView records the same numeric fields as a structured
 *       user event.
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
static inline void trace_task_delay_start(TCB_sctTCB_t *task, uint32_t delay_ticks) {}
static inline void trace_task_delay_end(TCB_sctTCB_t *task) {}

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
 * Mutex events
 * -------------------------------------------------------------------------- */
static inline void trace_mutex_create(const void *mutex) {}
static inline void trace_mutex_lock_enter(const void *mutex,
                                          TCB_sctTCB_t *task,
                                          TCB_sctTCB_t *owner,
                                          uint32_t timeout_ticks,
                                          uint8_t finite_timeout) {}
static inline void trace_mutex_lock_exit(const void *mutex,
                                         TCB_sctTCB_t *task,
                                         TCB_sctTCB_t *owner,
                                         uint8_t succeeded) {}
static inline void trace_mutex_block(const void *mutex,
                                     TCB_sctTCB_t *task,
                                     TCB_sctTCB_t *owner,
                                     uint32_t timeout_ticks,
                                     uint8_t finite_timeout) {}
static inline void trace_mutex_timeout(const void *mutex, TCB_sctTCB_t *task, TCB_sctTCB_t *owner) {
}
static inline void trace_mutex_unlock(const void *mutex,
                                      TCB_sctTCB_t *task,
                                      TCB_sctTCB_t *owner_before,
                                      TCB_sctTCB_t *owner_after,
                                      uint8_t succeeded) {}
static inline void trace_mutex_wake(const void *mutex, TCB_sctTCB_t *task) {}

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