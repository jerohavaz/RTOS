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
 * TeSSLa-compatible text stream over SEGGER RTT. SystemView task metadata is
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
 * @brief Initialize every enabled trace backend and clear trace-owned state.
 *
 * @details
 * RTT is initialized when required. The TeSSLa backend starts a new logical
 * stream with @c TESSLA_START. SystemView is configured using the project OS
 * API, whose task-list callback replays the trace-owned task registry.
 *
 * @pre Call exactly once during @c os_init(), before any task is created.
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
 * @brief Record a task-state transition for TeSSLa verification.
 *
 * @param task_id Numeric ID of the affected task.
 * @param old_state Previous task-state value.
 * @param new_state New task-state value.
 */
void trace_task_state(uint8_t task_id, uint8_t old_state, uint8_t new_state);

/* --------------------------------------------------------------------------
 * Scheduler events
 * -------------------------------------------------------------------------- */

/** @brief Record that @p task entered READY. */
void trace_task_ready(trace_task_ref_t task);

/** @brief Record that the scheduler started executing @p task. */
void trace_task_run(trace_task_ref_t task);

/** @brief Record that execution of the current non-idle task stopped. */
void trace_task_stop_run(void);

/** @brief Record that @p task entered BLOCKED. */
void trace_task_block(trace_task_ref_t task);

/** @brief Record that the scheduler selected the idle task. */
void trace_idle(void);

/**
 * @brief Record advancement of the kernel tick.
 * @param dt Number of elapsed kernel ticks represented by the event.
 */
void trace_tick(uint32_t dt);

/* --------------------------------------------------------------------------
 * ISR events
 * -------------------------------------------------------------------------- */

/** @brief Record entry into an instrumented ISR. */
void trace_isr_enter(void);

/** @brief Record an ISR exit that resumes normal interrupted execution. */
void trace_isr_exit(void);

/** @brief Record an ISR exit that transfers control to the scheduler. */
void trace_isr_exit_to_scheduler(void);

/* --------------------------------------------------------------------------
 * Delay events
 * -------------------------------------------------------------------------- */

/**
 * @brief Record the start of a busy-wait delay.
 * @param task Task performing the busy wait.
 * @param delay_ticks Requested duration in kernel ticks.
 */
void trace_task_delay_busy_start(trace_task_ref_t task, uint32_t delay_ticks);

/**
 * @brief Record completion of a busy-wait delay.
 * @param task Task completing the busy wait.
 */
void trace_task_delay_busy_end(trace_task_ref_t task);

/**
 * @brief Record the start of a scheduler-based delay.
 * @param task Task that will enter BLOCKED.
 * @param delay_ticks Requested duration in kernel ticks.
 */
void trace_task_delay_start(trace_task_ref_t task, uint32_t delay_ticks);

/**
 * @brief Record completion of a scheduler-based delay.
 * @param task Task whose delay expired.
 */
void trace_task_delay_end(trace_task_ref_t task);

/* --------------------------------------------------------------------------
 * Counting-semaphore events
 * -------------------------------------------------------------------------- */

/**
 * @brief Record creation of a counting semaphore.
 * @param semaphore Stable address of the semaphore object.
 * @param initial_count Initially available tokens.
 * @param max_count Maximum token count; one denotes a binary semaphore.
 */
void trace_sem_create(const void *semaphore, uint32_t initial_count, uint32_t max_count);

/**
 * @brief Record entry into a semaphore-acquire operation.
 * @param semaphore Semaphore being acquired.
 * @param task Calling task, or @ref trace_task_ref_none for exception context.
 * @param count Current semaphore count.
 * @param timeout_ticks Requested timeout value.
 * @param finite_timeout Nonzero when the timeout is finite.
 */
void trace_sem_acquire_enter(const void *semaphore,
                             trace_task_ref_t task,
                             uint32_t count,
                             uint32_t timeout_ticks,
                             uint8_t finite_timeout);

/**
 * @brief Record completion of a semaphore-acquire operation.
 * @param semaphore Semaphore being acquired.
 * @param task Calling task, or @ref trace_task_ref_none for exception context.
 * @param count Semaphore count after the attempt.
 * @param succeeded Nonzero when acquisition succeeded.
 */
void trace_sem_acquire_exit(const void *semaphore,
                            trace_task_ref_t task,
                            uint32_t count,
                            uint8_t succeeded);

/**
 * @brief Record that a task blocks while waiting for a semaphore.
 * @param semaphore Semaphore being waited on.
 * @param task Blocking task.
 * @param timeout_ticks Requested timeout value.
 * @param finite_timeout Nonzero when the timeout is finite.
 */
void trace_sem_block(const void *semaphore,
                     trace_task_ref_t task,
                     uint32_t timeout_ticks,
                     uint8_t finite_timeout);

/**
 * @brief Record expiration of a blocked semaphore acquire.
 * @param semaphore Semaphore being waited on.
 * @param task Task whose wait expired.
 * @param count Current semaphore count.
 */
void trace_sem_timeout(const void *semaphore, trace_task_ref_t task, uint32_t count);

/**
 * @brief Record a semaphore release attempt.
 * @param semaphore Semaphore being released.
 * @param count_before Count before release processing.
 * @param count_after Count after release processing.
 * @param max_count Configured maximum count.
 * @param succeeded Nonzero when release succeeded.
 */
void trace_sem_release(const void *semaphore,
                       uint32_t count_before,
                       uint32_t count_after,
                       uint32_t max_count,
                       uint8_t succeeded);

/**
 * @brief Record selection of a blocked task for semaphore wakeup.
 * @param semaphore Semaphore releasing the task.
 * @param task Task selected for wakeup.
 */
void trace_sem_wake(const void *semaphore, trace_task_ref_t task);

/* --------------------------------------------------------------------------
 * Mutex events
 * -------------------------------------------------------------------------- */

/**
 * @brief Record creation of a mutex.
 * @param mutex Stable address of the mutex object.
 */
void trace_mutex_create(const void *mutex);

/**
 * @brief Record entry into a mutex-lock operation.
 * @param mutex Mutex being locked.
 * @param task Calling task.
 * @param owner Current owner, or @ref trace_task_ref_none when unowned.
 * @param timeout_ticks Requested timeout value.
 * @param finite_timeout Nonzero when the timeout is finite.
 */
void trace_mutex_lock_enter(const void *mutex,
                            trace_task_ref_t task,
                            trace_task_ref_t owner,
                            uint32_t timeout_ticks,
                            uint8_t finite_timeout);

/**
 * @brief Record completion of a mutex-lock operation.
 * @param mutex Mutex being locked.
 * @param task Calling task.
 * @param owner Owner after the operation, or NONE when unowned.
 * @param succeeded Nonzero when the lock operation succeeded.
 */
void trace_mutex_lock_exit(const void *mutex,
                           trace_task_ref_t task,
                           trace_task_ref_t owner,
                           uint8_t succeeded);

/**
 * @brief Record that a task blocks waiting for a mutex.
 * @param mutex Mutex being waited on.
 * @param task Blocking task.
 * @param owner Current mutex owner.
 * @param timeout_ticks Requested timeout value.
 * @param finite_timeout Nonzero when the timeout is finite.
 */
void trace_mutex_block(const void *mutex,
                       trace_task_ref_t task,
                       trace_task_ref_t owner,
                       uint32_t timeout_ticks,
                       uint8_t finite_timeout);

/**
 * @brief Record expiration of a blocked mutex lock.
 * @param mutex Mutex being waited on.
 * @param task Task whose wait expired.
 * @param owner Current owner when the timeout is processed.
 */
void trace_mutex_timeout(const void *mutex, trace_task_ref_t task, trace_task_ref_t owner);

/**
 * @brief Record a mutex-unlock operation.
 * @param mutex Mutex being unlocked.
 * @param task Calling task.
 * @param owner_before Owner before the unlock attempt.
 * @param owner_after Owner after the unlock attempt.
 * @param succeeded Nonzero when the unlock succeeded.
 */
void trace_mutex_unlock(const void *mutex,
                        trace_task_ref_t task,
                        trace_task_ref_t owner_before,
                        trace_task_ref_t owner_after,
                        uint8_t succeeded);

/**
 * @brief Record selection of a blocked task for mutex wakeup/handoff.
 * @param mutex Mutex whose ownership is handed off.
 * @param task Task selected for wakeup.
 */
void trace_mutex_wake(const void *mutex, trace_task_ref_t task);

/* --------------------------------------------------------------------------
 * Message-queue events
 * -------------------------------------------------------------------------- */

/** @brief Record queue creation. */
void trace_queue_create(uint32_t queue_id, uint32_t capacity);

/** @brief Record a queue-send attempt. */
void trace_queue_send_attempt(uint32_t queue_id,
                              uint8_t task_id,
                              uint8_t task_priority,
                              uint32_t timeout_ticks,
                              uint32_t message_hash);

/** @brief Record successful completion of a queue send. */
void trace_queue_send_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash);

/** @brief Record that a sender blocks on a full queue. */
void trace_queue_send_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority);

/** @brief Record expiration of a blocked queue send. */
void trace_queue_send_timeout(uint32_t queue_id, uint8_t task_id);

/** @brief Record a queue-receive attempt. */
void trace_queue_receive_attempt(uint32_t queue_id,
                                 uint8_t task_id,
                                 uint8_t task_priority,
                                 uint32_t timeout_ticks);

/** @brief Record successful completion of a queue receive. */
void trace_queue_receive_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash);

/** @brief Record that a receiver blocks on an empty queue. */
void trace_queue_receive_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority);

/** @brief Record expiration of a blocked queue receive. */
void trace_queue_receive_timeout(uint32_t queue_id, uint8_t task_id);

/** @brief Record selection of a blocked sender for wakeup. */
void trace_queue_wake_sender(uint32_t queue_id, uint8_t task_id);

/** @brief Record selection of a blocked receiver for wakeup. */
void trace_queue_wake_receiver(uint32_t queue_id, uint8_t task_id);

/**
 * @brief Record a direct queue handoff that bypasses the ring buffer.
 * @param queue_id Queue coordinating the transfer.
 * @param sender_id Sending task ID or @ref TRACE_TASK_ID_NONE.
 * @param receiver_id Receiving task ID or @ref TRACE_TASK_ID_NONE.
 * @param message_hash Hash of the transferred message.
 */
void trace_queue_handoff(uint32_t queue_id,
                         uint8_t sender_id,
                         uint8_t receiver_id,
                         uint32_t message_hash);

/** @brief Record the queue fill level after a ring-buffer modification. */
void trace_queue_fill(uint32_t queue_id, uint32_t fill);

/* --------------------------------------------------------------------------
 * Generic log event
 * -------------------------------------------------------------------------- */

/**
 * @brief Emit a generic null-terminated trace log message.
 * @param text Message to emit; a null pointer is ignored.
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
 * Generic log event
 * -------------------------------------------------------------------------- */
static inline void trace_log(const char *text) {}

#endif /* OS_TRACE_ENABLED */