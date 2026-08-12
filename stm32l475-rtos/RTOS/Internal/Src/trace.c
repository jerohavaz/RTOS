/**
 * @file trace.c
 * @brief Configurable kernel trace-backend implementation.
 * @author Jerome
 */

#include "trace.h"

#if OS_TRACE_ENABLED

#include "kernel_panic.h"
#include "port.h"

#if OS_TRACE_SEGGER_SYSVIEW || OS_TRACE_TESSLA_RTT
#include "SEGGER_RTT.h"
#include <stdio.h>
#endif

#if OS_TRACE_SEGGER_SYSVIEW
#include "SEGGER_SYSVIEW.h"

/**
 * @brief Custom SystemView OS-API event identifiers.
 *
 * SystemView reserves IDs 0..31 for built-in events. IDs 32..511 are
 * available for OS API instrumentation and are decoded on the host using
 * @c SYSVIEW_CustomRTOS.txt. Keeping the descriptions host-side avoids
 * target-side description callbacks and their measurement jitter.
 *
 * Task state changes and SysTick are intentionally not duplicated here:
 * SystemView already represents task Ready/Run/Block/Idle and ISR timing with
 * native events. TeSSLa still receives explicit STATE and TICK records.
 */
typedef enum {
    TRACE_SV_EVT_DELAY_BUSY_START = 32u,
    TRACE_SV_EVT_DELAY_BUSY_END,
    TRACE_SV_EVT_DELAY_START,
    TRACE_SV_EVT_DELAY_END,

    TRACE_SV_EVT_SEM_CREATE,
    TRACE_SV_EVT_SEM_ACQUIRE_ENTER,
    TRACE_SV_EVT_SEM_ACQUIRE_EXIT,
    TRACE_SV_EVT_SEM_BLOCK,
    TRACE_SV_EVT_SEM_TIMEOUT,
    TRACE_SV_EVT_SEM_RELEASE,
    TRACE_SV_EVT_SEM_WAKE,

    TRACE_SV_EVT_MUTEX_CREATE,
    TRACE_SV_EVT_MUTEX_LOCK_ENTER,
    TRACE_SV_EVT_MUTEX_LOCK_EXIT,
    TRACE_SV_EVT_MUTEX_BLOCK,
    TRACE_SV_EVT_MUTEX_TIMEOUT,
    TRACE_SV_EVT_MUTEX_UNLOCK,
    TRACE_SV_EVT_MUTEX_WAKE,

    TRACE_SV_EVT_QUEUE_CREATE,
    TRACE_SV_EVT_QUEUE_SEND_ATTEMPT,
    TRACE_SV_EVT_QUEUE_SEND_SUCCESS,
    TRACE_SV_EVT_QUEUE_SEND_BLOCK,
    TRACE_SV_EVT_QUEUE_SEND_TIMEOUT,
    TRACE_SV_EVT_QUEUE_RECV_ATTEMPT,
    TRACE_SV_EVT_QUEUE_RECV_SUCCESS,
    TRACE_SV_EVT_QUEUE_RECV_BLOCK,
    TRACE_SV_EVT_QUEUE_RECV_TIMEOUT,
    TRACE_SV_EVT_QUEUE_WAKE_SEND,
    TRACE_SV_EVT_QUEUE_WAKE_RECV,
    TRACE_SV_EVT_QUEUE_HANDOFF,
    TRACE_SV_EVT_QUEUE_FILL
} trace_sysview_event_id_t;
#endif

#if OS_TRACE_TESSLA_RTT
#include <stdarg.h>
#include <stddef.h>

#define TRACE_TESSLA_RTT_CHANNEL  (0u)
#define TRACE_TESSLA_PAYLOAD_SIZE (96u)
#define TRACE_TESSLA_RECORD_SIZE  (128u)

static uint32_t g_trace_sequence;

/**
 * @brief Format and submit one logical TeSSLa event as an RTT record.
 *
 * Sequence allocation and RTT insertion occur within the same critical
 * section, preventing task and SysTick producers from appearing out of order.
 *
 * RTT remains non-blocking. If the complete record does not fit, it is
 * discarded. Because its sequence number has already been consumed, the
 * receiver detects the loss when the next record arrives.
 *
 * @param format printf-style format string for the event payload.
 * @param ... Arguments referenced by @p format.
 */
static void trace_tessla_emit(const char *format, ...) {
    char payload[TRACE_TESSLA_PAYLOAD_SIZE];
    char record[TRACE_TESSLA_RECORD_SIZE];

    va_list args;
    va_start(args, format);
    int payload_length = vsnprintf(payload, sizeof(payload), format, args);
    va_end(args);

    if (payload_length < 0 || (size_t)payload_length >= sizeof(payload)) {
        return;
    }

    uint32_t key = port_enter_critical();
    uint32_t sequence = g_trace_sequence++;

    int record_length =
        snprintf(record, sizeof(record), "TRACE %lu %s\n", (unsigned long)sequence, payload);

    if (record_length > 0 && (size_t)record_length < sizeof(record)) {
        SEGGER_RTT_WriteSkipNoLock(TRACE_TESSLA_RTT_CHANNEL, record, (unsigned int)record_length);
    }

    port_exit_critical(key);
}
#endif

#if OS_TRACE_SEGGER_SYSVIEW && (OS_TRACE_TASKS || OS_TRACE_SCHEDULER)
/**
 * @brief Convert an RTOS task ID to the SystemView task-ID type.
 *
 * @param task Task control block whose ID is required.
 *
 * @return Task ID converted to @c U32.
 *
 * @pre @p task must not be null.
 */
static U32 sv_task_id(const TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);
    return (U32)(uintptr_t)task;
}
#endif

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_TASKS
/**
 * @brief Send task metadata to SEGGER SystemView.
 *
 * Reports the task's SystemView identifier, numeric task ID as its display
 * name, priority, and stack bounds. Stack usage is reported as unknown because
 * the kernel does not currently track the stack high-water mark.
 *
 * @param task Task control block whose metadata will be reported.
 *
 * @pre @p task must not be null.
 * @note SEGGER_SYSVIEW_SendTaskInfo() encodes the task name immediately, so the
 *       local name buffer does not need to persist after this function returns.
 */
static void sv_send_task_info(const TCB_sctTCB_t *task) {
    char name[4]; /* "255" plus '\0' */

    snprintf(name, sizeof(name), "%u", (unsigned int)task->u8TaskId);

    const SEGGER_SYSVIEW_TASKINFO info = {
        .TaskID = sv_task_id(task),
        .sName = name,
        .Prio = task->u8TaskPrio,
        .StackBase = (U32)(uintptr_t)task->au32TaskStack,
        .StackSize = (U32)sizeof(task->au32TaskStack),
        .StackUsage = 0u,
    };

    SEGGER_SYSVIEW_SendTaskInfo(&info);
}
#endif

void trace_init(void) {
#if OS_TRACE_SEGGER_SYSVIEW || OS_TRACE_TESSLA_RTT
    /*
     * Both tracing backends use RTT. Explicit initialization is required
     * because the NOLOAD RTT control block can retain its contents across
     * target resets.
     */
    SEGGER_RTT_Init();
#endif

#if OS_TRACE_TESSLA_RTT
    g_trace_sequence = 0u;
    SEGGER_RTT_WriteString(TRACE_TESSLA_RTT_CHANNEL, "TESSLA_START\n");
#endif

#if OS_TRACE_SEGGER_SYSVIEW
    SEGGER_SYSVIEW_Conf();
    SEGGER_SYSVIEW_Start();
#endif
}

/* --------------------------------------------------------------------------
 * Task events
 * -------------------------------------------------------------------------- */

void trace_task_create(const TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_TASKS
    SEGGER_SYSVIEW_OnTaskCreate(sv_task_id(task));
    sv_send_task_info(task);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_TASKS
    trace_tessla_emit(
        "TASK_CREATE %u %u", (unsigned int)task->u8TaskId, (unsigned int)task->u8TaskPrio);
#endif
}

void trace_task_state(uint8_t task_id, uint8_t old_state, uint8_t new_state) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_TASKS
    trace_tessla_emit(
        "STATE %u %u %u", (unsigned int)task_id, (unsigned int)old_state, (unsigned int)new_state);
#endif
}

/* --------------------------------------------------------------------------
 * Scheduler events
 * -------------------------------------------------------------------------- */

void trace_task_ready(const TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    SEGGER_SYSVIEW_OnTaskStartReady(sv_task_id(task));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    trace_tessla_emit("READY %u %u", (unsigned int)task->u8TaskId, (unsigned int)task->u8TaskPrio);
#endif
}

void trace_task_run(const TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    SEGGER_SYSVIEW_OnTaskStartExec(sv_task_id(task));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    trace_tessla_emit(
        "RUNNING %u %u", (unsigned int)task->u8TaskId, (unsigned int)task->u8TaskPrio);
#endif
}

void trace_task_stop_run(void) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    SEGGER_SYSVIEW_OnTaskStopExec();
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    trace_tessla_emit("STOP_RUNNING");
#endif
}

void trace_task_block(const TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    SEGGER_SYSVIEW_OnTaskStopReady(sv_task_id(task), 0u);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    trace_tessla_emit("BLOCKED %u", (unsigned int)task->u8TaskId);
#endif
}

void trace_idle(void) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    SEGGER_SYSVIEW_OnIdle();
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    trace_tessla_emit("IDLE");
#endif
}

void trace_tick(uint32_t dt) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    trace_tessla_emit("TICK %lu", (unsigned long)dt);
#endif
}

/* --------------------------------------------------------------------------
 * ISR events
 * -------------------------------------------------------------------------- */

void trace_isr_enter(void) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_ISR
    SEGGER_SYSVIEW_RecordEnterISR();
#endif
}

void trace_isr_exit(void) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_ISR
    SEGGER_SYSVIEW_RecordExitISR();
#endif
}

void trace_isr_exit_to_scheduler(void) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_ISR
    SEGGER_SYSVIEW_RecordExitISRToScheduler();
#endif
}

/* --------------------------------------------------------------------------
 * Delay events
 * -------------------------------------------------------------------------- */

void trace_task_delay_busy_start(const TCB_sctTCB_t *task, uint32_t delay_ticks) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_DELAY
    SEGGER_SYSVIEW_RecordU32x2(
        TRACE_SV_EVT_DELAY_BUSY_START, (U32)task->u8TaskId, (U32)delay_ticks);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_DELAY
    trace_tessla_emit(
        "DELAY_BUSY_START %u %u", (unsigned int)task->u8TaskId, (unsigned int)delay_ticks);
#endif
}

void trace_task_delay_busy_end(const TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_DELAY
    SEGGER_SYSVIEW_RecordU32(TRACE_SV_EVT_DELAY_BUSY_END, (U32)task->u8TaskId);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_DELAY
    trace_tessla_emit("DELAY_BUSY_END %u", (unsigned int)task->u8TaskId);
#endif
}

void trace_task_delay_start(const TCB_sctTCB_t *task, uint32_t delay_ticks) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_DELAY
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_DELAY_START, (U32)task->u8TaskId, (U32)delay_ticks);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_DELAY
    trace_tessla_emit("DELAY_START %u %u", (unsigned int)task->u8TaskId, (unsigned int)delay_ticks);
#endif
}

void trace_task_delay_end(const TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_DELAY
    SEGGER_SYSVIEW_RecordU32(TRACE_SV_EVT_DELAY_END, (U32)task->u8TaskId);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_DELAY
    trace_tessla_emit("DELAY_END %u", (unsigned int)task->u8TaskId);
#endif
}

/* --------------------------------------------------------------------------
 * Counting-semaphore events
 * -------------------------------------------------------------------------- */

#if (OS_TRACE_SEGGER_SYSVIEW || OS_TRACE_TESSLA_RTT) && OS_TRACE_SEMAPHORE
/**
 * @brief Convert a semaphore address to its numeric trace identifier.
 *
 * The semaphore address remains stable for the object's lifetime and allows
 * the verifier to correlate events belonging to the same semaphore.
 *
 * @param semaphore Semaphore object whose trace identifier is required.
 *
 * @return Address of @p semaphore represented as an unsigned integer.
 *
 * @pre @p semaphore must not be null.
 */
static unsigned long trace_sem_id(const void *semaphore) {
    KERNEL_REQUIRE(semaphore != 0);
    return (unsigned long)(uintptr_t)semaphore;
}

/**
 * @brief Convert an optional task control block to its numeric trace ID.
 *
 * A null task represents an operation without an associated task, such as an
 * acquire attempted from exception context. It is encoded as @c UINT8_MAX so
 * it remains distinguishable from every valid kernel task ID.
 *
 * @param task Task control block to encode, or null when no task is associated
 *             with the operation.
 *
 * @return @p task's kernel task ID, or @c UINT8_MAX when @p task is null.
 */
static unsigned int trace_sem_task_id(const TCB_sctTCB_t *task) {
    return (task != 0) ? (unsigned int)task->u8TaskId : (unsigned int)UINT8_MAX;
}
#endif

void trace_sem_create(const void *semaphore, uint32_t initial_count, uint32_t max_count) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_SEM_CREATE, (U32)trace_sem_id(semaphore), (U32)initial_count, (U32)max_count);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_emit("SEM_CREATE %lu %lu %lu",
                      trace_sem_id(semaphore),
                      (unsigned long)initial_count,
                      (unsigned long)max_count);
#endif
}

void trace_sem_acquire_enter(const void *semaphore,
                             const TCB_sctTCB_t *task,
                             uint32_t count,
                             uint32_t timeout_ticks,
                             uint8_t finite_timeout) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x5(TRACE_SV_EVT_SEM_ACQUIRE_ENTER,
                               (U32)trace_sem_id(semaphore),
                               (U32)trace_sem_task_id(task),
                               (U32)count,
                               (U32)timeout_ticks,
                               (U32)(finite_timeout != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_emit("SEM_ACQUIRE_ENTER %lu %u %lu %lu %u",
                      trace_sem_id(semaphore),
                      trace_sem_task_id(task),
                      (unsigned long)count,
                      (unsigned long)timeout_ticks,
                      (unsigned int)(finite_timeout != 0u));
#endif
}

void trace_sem_acquire_exit(const void *semaphore,
                            const TCB_sctTCB_t *task,
                            uint32_t count,
                            uint8_t succeeded) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x4(TRACE_SV_EVT_SEM_ACQUIRE_EXIT,
                               (U32)trace_sem_id(semaphore),
                               (U32)trace_sem_task_id(task),
                               (U32)count,
                               (U32)(succeeded != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_emit("SEM_ACQUIRE_EXIT %lu %u %lu %u",
                      trace_sem_id(semaphore),
                      trace_sem_task_id(task),
                      (unsigned long)count,
                      (unsigned int)(succeeded != 0u));
#endif
}

void trace_sem_block(const void *semaphore,
                     const TCB_sctTCB_t *task,
                     uint32_t timeout_ticks,
                     uint8_t finite_timeout) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x5(TRACE_SV_EVT_SEM_BLOCK,
                               (U32)trace_sem_id(semaphore),
                               (U32)task->u8TaskId,
                               (U32)task->u8TaskPrio,
                               (U32)timeout_ticks,
                               (U32)(finite_timeout != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_emit("SEM_BLOCK %lu %u %u %lu %u",
                      trace_sem_id(semaphore),
                      (unsigned int)task->u8TaskId,
                      (unsigned int)task->u8TaskPrio,
                      (unsigned long)timeout_ticks,
                      (unsigned int)(finite_timeout != 0u));
#endif
}

void trace_sem_timeout(const void *semaphore, const TCB_sctTCB_t *task, uint32_t count) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_SEM_TIMEOUT, (U32)trace_sem_id(semaphore), (U32)task->u8TaskId, (U32)count);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_emit("SEM_TIMEOUT %lu %u %lu",
                      trace_sem_id(semaphore),
                      (unsigned int)task->u8TaskId,
                      (unsigned long)count);
#endif
}

void trace_sem_release(const void *semaphore,
                       uint32_t count_before,
                       uint32_t count_after,
                       uint32_t max_count,
                       uint8_t succeeded) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x5(TRACE_SV_EVT_SEM_RELEASE,
                               (U32)trace_sem_id(semaphore),
                               (U32)count_before,
                               (U32)count_after,
                               (U32)max_count,
                               (U32)(succeeded != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_emit("SEM_RELEASE %lu %lu %lu %lu %u",
                      trace_sem_id(semaphore),
                      (unsigned long)count_before,
                      (unsigned long)count_after,
                      (unsigned long)max_count,
                      (unsigned int)(succeeded != 0u));
#endif
}

void trace_sem_wake(const void *semaphore, const TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x3(TRACE_SV_EVT_SEM_WAKE,
                               (U32)trace_sem_id(semaphore),
                               (U32)task->u8TaskId,
                               (U32)task->u8TaskPrio);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_emit("SEM_WAKE %lu %u %u",
                      trace_sem_id(semaphore),
                      (unsigned int)task->u8TaskId,
                      (unsigned int)task->u8TaskPrio);
#endif
}

/* --------------------------------------------------------------------------
 * Mutex events
 * -------------------------------------------------------------------------- */

#if (OS_TRACE_SEGGER_SYSVIEW || OS_TRACE_TESSLA_RTT) && OS_TRACE_MUTEX
/**
 * @brief Convert a mutex address to its numeric trace identifier.
 *
 * The mutex object address is stable for the object's lifetime and allows the
 * verifier to correlate all events belonging to the same mutex. The verifier
 * may map this runtime address to a bounded internal monitor slot.
 *
 * @param mutex Mutex object whose trace identifier is required.
 *
 * @return Address of @p mutex represented as an unsigned integer.
 *
 * @pre @p mutex must not be null.
 */
static unsigned long trace_mutex_id(const void *mutex) {
    KERNEL_REQUIRE(mutex != 0);
    return (unsigned long)(uintptr_t)mutex;
}

/**
 * @brief Convert an optional task control block to its numeric trace ID.
 *
 * A null task represents an unowned mutex or an operation without an owning
 * task. Such cases are encoded as @c UINT8_MAX so they remain distinguishable
 * from every valid kernel task ID.
 *
 * @param task Task control block to encode, or null when no task is present.
 *
 * @return @p task's kernel task ID, or @c UINT8_MAX when @p task is null.
 */
static unsigned int trace_mutex_task_id(const TCB_sctTCB_t *task) {
    return (task != 0) ? (unsigned int)task->u8TaskId : (unsigned int)UINT8_MAX;
}
#endif

void trace_mutex_create(const void *mutex) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32(TRACE_SV_EVT_MUTEX_CREATE, (U32)trace_mutex_id(mutex));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_emit("MUTEX_CREATE %lu", trace_mutex_id(mutex));
#endif
}

void trace_mutex_lock_enter(const void *mutex,
                            const TCB_sctTCB_t *task,
                            const TCB_sctTCB_t *owner,
                            uint32_t timeout_ticks,
                            uint8_t finite_timeout) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32x5(TRACE_SV_EVT_MUTEX_LOCK_ENTER,
                               (U32)trace_mutex_id(mutex),
                               (U32)trace_mutex_task_id(task),
                               (U32)trace_mutex_task_id(owner),
                               (U32)timeout_ticks,
                               (U32)(finite_timeout != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_emit("MUTEX_LOCK_ENTER %lu %u %u %lu %u",
                      trace_mutex_id(mutex),
                      trace_mutex_task_id(task),
                      trace_mutex_task_id(owner),
                      (unsigned long)timeout_ticks,
                      (unsigned int)(finite_timeout != 0u));
#endif
}

void trace_mutex_lock_exit(const void *mutex,
                           const TCB_sctTCB_t *task,
                           const TCB_sctTCB_t *owner,
                           uint8_t succeeded) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32x4(TRACE_SV_EVT_MUTEX_LOCK_EXIT,
                               (U32)trace_mutex_id(mutex),
                               (U32)trace_mutex_task_id(task),
                               (U32)trace_mutex_task_id(owner),
                               (U32)(succeeded != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_emit("MUTEX_LOCK_EXIT %lu %u %u %u",
                      trace_mutex_id(mutex),
                      trace_mutex_task_id(task),
                      trace_mutex_task_id(owner),
                      (unsigned int)(succeeded != 0u));
#endif
}

void trace_mutex_block(const void *mutex,
                       const TCB_sctTCB_t *task,
                       const TCB_sctTCB_t *owner,
                       uint32_t timeout_ticks,
                       uint8_t finite_timeout) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32x6(TRACE_SV_EVT_MUTEX_BLOCK,
                               (U32)trace_mutex_id(mutex),
                               (U32)task->u8TaskId,
                               (U32)task->u8TaskPrio,
                               (U32)trace_mutex_task_id(owner),
                               (U32)timeout_ticks,
                               (U32)(finite_timeout != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_emit("MUTEX_BLOCK %lu %u %u %u %lu %u",
                      trace_mutex_id(mutex),
                      (unsigned int)task->u8TaskId,
                      (unsigned int)task->u8TaskPrio,
                      trace_mutex_task_id(owner),
                      (unsigned long)timeout_ticks,
                      (unsigned int)(finite_timeout != 0u));
#endif
}

void trace_mutex_timeout(const void *mutex, const TCB_sctTCB_t *task, const TCB_sctTCB_t *owner) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32x3(TRACE_SV_EVT_MUTEX_TIMEOUT,
                               (U32)trace_mutex_id(mutex),
                               (U32)task->u8TaskId,
                               (U32)trace_mutex_task_id(owner));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_emit("MUTEX_TIMEOUT %lu %u %u",
                      trace_mutex_id(mutex),
                      (unsigned int)task->u8TaskId,
                      trace_mutex_task_id(owner));
#endif
}

void trace_mutex_unlock(const void *mutex,
                        const TCB_sctTCB_t *task,
                        const TCB_sctTCB_t *owner_before,
                        const TCB_sctTCB_t *owner_after,
                        uint8_t succeeded) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32x5(TRACE_SV_EVT_MUTEX_UNLOCK,
                               (U32)trace_mutex_id(mutex),
                               (U32)trace_mutex_task_id(task),
                               (U32)trace_mutex_task_id(owner_before),
                               (U32)trace_mutex_task_id(owner_after),
                               (U32)(succeeded != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_emit("MUTEX_UNLOCK %lu %u %u %u %u",
                      trace_mutex_id(mutex),
                      trace_mutex_task_id(task),
                      trace_mutex_task_id(owner_before),
                      trace_mutex_task_id(owner_after),
                      (unsigned int)(succeeded != 0u));
#endif
}

void trace_mutex_wake(const void *mutex, const TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32x3(TRACE_SV_EVT_MUTEX_WAKE,
                               (U32)trace_mutex_id(mutex),
                               (U32)task->u8TaskId,
                               (U32)task->u8TaskPrio);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_emit("MUTEX_WAKE %lu %u %u",
                      trace_mutex_id(mutex),
                      (unsigned int)task->u8TaskId,
                      (unsigned int)task->u8TaskPrio);
#endif
}

/* --------------------------------------------------------------------------
 * Message queue events
 * -------------------------------------------------------------------------- */

void trace_queue_create(uint32_t queue_id, uint32_t capacity) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_QUEUE_CREATE, (U32)queue_id, (U32)capacity);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_CREATE %lu %lu", (unsigned long)queue_id, (unsigned long)capacity);
#endif
}

void trace_queue_send_attempt(uint32_t queue_id,
                              uint8_t task_id,
                              uint8_t task_priority,
                              uint32_t timeout_ticks,
                              uint32_t message_hash) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x5(TRACE_SV_EVT_QUEUE_SEND_ATTEMPT,
                               (U32)queue_id,
                               (U32)task_id,
                               (U32)task_priority,
                               (U32)timeout_ticks,
                               (U32)message_hash);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_SEND_ATTEMPT %lu %u %u %lu %lu",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned int)task_priority,
                      (unsigned long)timeout_ticks,
                      (unsigned long)message_hash);
#endif
}

void trace_queue_send_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_QUEUE_SEND_SUCCESS, (U32)queue_id, (U32)task_id, (U32)message_hash);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_SEND_SUCCESS %lu %u %lu",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned long)message_hash);
#endif
}

void trace_queue_send_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_QUEUE_SEND_BLOCK, (U32)queue_id, (U32)task_id, (U32)task_priority);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_SEND_BLOCK %lu %u %u",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned int)task_priority);
#endif
}

void trace_queue_send_timeout(uint32_t queue_id, uint8_t task_id) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_QUEUE_SEND_TIMEOUT, (U32)queue_id, (U32)task_id);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_SEND_TIMEOUT %lu %u", (unsigned long)queue_id, (unsigned int)task_id);
#endif
}

void trace_queue_receive_attempt(uint32_t queue_id,
                                 uint8_t task_id,
                                 uint8_t task_priority,
                                 uint32_t timeout_ticks) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x4(TRACE_SV_EVT_QUEUE_RECV_ATTEMPT,
                               (U32)queue_id,
                               (U32)task_id,
                               (U32)task_priority,
                               (U32)timeout_ticks);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_RECV_ATTEMPT %lu %u %u %lu",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned int)task_priority,
                      (unsigned long)timeout_ticks);
#endif
}

void trace_queue_receive_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_QUEUE_RECV_SUCCESS, (U32)queue_id, (U32)task_id, (U32)message_hash);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_RECV_SUCCESS %lu %u %lu",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned long)message_hash);
#endif
}

void trace_queue_receive_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_QUEUE_RECV_BLOCK, (U32)queue_id, (U32)task_id, (U32)task_priority);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_RECV_BLOCK %lu %u %u",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned int)task_priority);
#endif
}

void trace_queue_receive_timeout(uint32_t queue_id, uint8_t task_id) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_QUEUE_RECV_TIMEOUT, (U32)queue_id, (U32)task_id);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_RECV_TIMEOUT %lu %u", (unsigned long)queue_id, (unsigned int)task_id);
#endif
}

void trace_queue_wake_sender(uint32_t queue_id, uint8_t task_id) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_QUEUE_WAKE_SEND, (U32)queue_id, (U32)task_id);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_WAKE_SEND %lu %u", (unsigned long)queue_id, (unsigned int)task_id);
#endif
}

void trace_queue_wake_receiver(uint32_t queue_id, uint8_t task_id) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_QUEUE_WAKE_RECV, (U32)queue_id, (U32)task_id);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_WAKE_RECV %lu %u", (unsigned long)queue_id, (unsigned int)task_id);
#endif
}

void trace_queue_handoff(uint32_t queue_id,
                         uint8_t sender_id,
                         uint8_t receiver_id,
                         uint32_t message_hash) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x4(TRACE_SV_EVT_QUEUE_HANDOFF,
                               (U32)queue_id,
                               (U32)sender_id,
                               (U32)receiver_id,
                               (U32)message_hash);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_HANDOFF %lu %u %u %lu",
                      (unsigned long)queue_id,
                      (unsigned int)sender_id,
                      (unsigned int)receiver_id,
                      (unsigned long)message_hash);
#endif
}

void trace_queue_fill(uint32_t queue_id, uint32_t fill) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_QUEUE_FILL, (U32)queue_id, (U32)fill);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_FILL %lu %lu", (unsigned long)queue_id, (unsigned long)fill);
#endif
}

/* --------------------------------------------------------------------------
 * Generic log event
 * -------------------------------------------------------------------------- */

void trace_log(const char *text) {
#if OS_TRACE_SEGGER_SYSVIEW
    if (text != 0) {
        SEGGER_SYSVIEW_Print(text);
    }
#endif

#if OS_TRACE_TESSLA_RTT
    if (text != 0) {
        trace_tessla_emit("LOG %s", text);
    }
#endif
}

/**
 * @brief Return the active Cortex-M exception ID to SystemView.
 *
 * SystemView configuration code may call this function even when explicit RTOS
 * ISR tracing is disabled, so it is available whenever the SystemView backend
 * is enabled.
 *
 * @return Active exception number, or zero in Thread mode.
 */
#if OS_TRACE_SEGGER_SYSVIEW
U32 SEGGER_SYSVIEW_X_GetInterruptId(void) {
    return (U32)port_get_active_exception_id();
}
#endif

#endif /* OS_TRACE_ENABLED */