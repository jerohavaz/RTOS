/**
 * @file trace.c
 * @brief Configurable kernel trace-backend implementation.
 * @author Jerome
 */

#include "trace.h"

#if OS_TRACE_ENABLED

#include "kernel_panic.h"
#include "port.h"

#if OS_TRACE_SEGGER_SYSVIEW
#include "SEGGER_SYSVIEW.h"
#endif

#if OS_TRACE_TESSLA_RTT
#include "SEGGER_RTT.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

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
    return (U32)task->u8TaskId;
}
#endif

void trace_init(void) {
#if OS_TRACE_TESSLA_RTT
    SEGGER_RTT_Init();
    g_trace_sequence = 0u;
    SEGGER_RTT_WriteString(TRACE_TESSLA_RTT_CHANNEL, "TESSLA_START\n");
#endif

#if OS_TRACE_SEGGER_SYSVIEW
    SEGGER_SYSVIEW_Conf();
    SEGGER_SYSVIEW_Print("SYSVIEW started");
#endif
}

/* --------------------------------------------------------------------------
 * Task events
 * -------------------------------------------------------------------------- */

void trace_task_create(TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_TASKS
    SEGGER_SYSVIEW_OnTaskCreate(sv_task_id(task));
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

void trace_task_ready(TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    SEGGER_SYSVIEW_OnTaskStartReady(sv_task_id(task));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    trace_tessla_emit("READY %u %u", (unsigned int)task->u8TaskId, (unsigned int)task->u8TaskPrio);
#endif
}

void trace_task_run(TCB_sctTCB_t *task) {
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

void trace_task_block(TCB_sctTCB_t *task) {
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
 * Non-Blocking Delay events
 * -------------------------------------------------------------------------- */

void trace_task_delay_busy_start(TCB_sctTCB_t *task, uint32_t delay_ticks) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_TESSLA_RTT && OS_TRACE_DELAY
    trace_tessla_emit(
        "DELAY_BUSY_START %u %u", (unsigned int)task->u8TaskId, (unsigned int)delay_ticks);
#endif
}

void trace_task_delay_busy_end(TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_TESSLA_RTT && OS_TRACE_DELAY
    trace_tessla_emit("DELAY_BUSY_END %u", (unsigned int)task->u8TaskId);
#endif
}

/* --------------------------------------------------------------------------
 * Counting-semaphore events
 * -------------------------------------------------------------------------- */

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
static unsigned long trace_sem_id(const void *semaphore) {
    KERNEL_REQUIRE(semaphore != 0);
    return (unsigned long)(uintptr_t)semaphore;
}

/** Task-ID value used when an acquire has no owning task. */
static unsigned int trace_sem_task_id(const TCB_sctTCB_t *task) {
    return (task != 0) ? (unsigned int)task->u8TaskId : (unsigned int)UINT8_MAX;
}
#endif

void trace_sem_create(const void *semaphore, uint32_t initial_count, uint32_t max_count) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_emit("SEM_CREATE %lu %lu %lu",
                      trace_sem_id(semaphore),
                      (unsigned long)initial_count,
                      (unsigned long)max_count);
#endif
}

void trace_sem_acquire_enter(const void *semaphore,
                             TCB_sctTCB_t *task,
                             uint32_t count,
                             uint32_t timeout_ticks,
                             uint8_t finite_timeout) {
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
                            TCB_sctTCB_t *task,
                            uint32_t count,
                            uint8_t succeeded) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_emit("SEM_ACQUIRE_EXIT %lu %u %lu %u",
                      trace_sem_id(semaphore),
                      trace_sem_task_id(task),
                      (unsigned long)count,
                      (unsigned int)(succeeded != 0u));
#endif
}

void trace_sem_block(const void *semaphore,
                     TCB_sctTCB_t *task,
                     uint32_t timeout_ticks,
                     uint8_t finite_timeout) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_emit("SEM_BLOCK %lu %u %u %lu %u",
                      trace_sem_id(semaphore),
                      (unsigned int)task->u8TaskId,
                      (unsigned int)task->u8TaskPrio,
                      (unsigned long)timeout_ticks,
                      (unsigned int)(finite_timeout != 0u));
#endif
}

void trace_sem_timeout(const void *semaphore, TCB_sctTCB_t *task, uint32_t count) {
    KERNEL_REQUIRE(task != 0);

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
#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_emit("SEM_RELEASE %lu %lu %lu %lu %u",
                      trace_sem_id(semaphore),
                      (unsigned long)count_before,
                      (unsigned long)count_after,
                      (unsigned long)max_count,
                      (unsigned int)(succeeded != 0u));
#endif
}

void trace_sem_wake(const void *semaphore, TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_emit("SEM_WAKE %lu %u %u",
                      trace_sem_id(semaphore),
                      (unsigned int)task->u8TaskId,
                      (unsigned int)task->u8TaskPrio);
#endif
}

/* --------------------------------------------------------------------------
 * Message queue events
 * -------------------------------------------------------------------------- */

void trace_queue_create(uint32_t queue_id, uint32_t capacity) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_CREATE %lu %lu\n", (unsigned long)queue_id, (unsigned long)capacity);
#endif
}

void trace_queue_send_attempt(uint32_t queue_id,
                              uint8_t task_id,
                              uint8_t task_priority,
                              uint32_t timeout_ticks,
                              uint32_t message_hash) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_SEND_ATTEMPT %lu %u %u %lu %lu\n",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned int)task_priority,
                      (unsigned long)timeout_ticks,
                      (unsigned long)message_hash);
#endif
}

void trace_queue_send_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_SEND_SUCCESS %lu %u %lu\n",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned long)message_hash);
#endif
}

void trace_queue_send_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_SEND_BLOCK %lu %u %u\n",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned int)task_priority);
#endif
}

void trace_queue_send_timeout(uint32_t queue_id, uint8_t task_id) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    SEGGER_RTT_printf(
        0, "QUEUE_SEND_TIMEOUT %lu %u\n", (unsigned long)queue_id, (unsigned int)task_id);
#endif
}

void trace_queue_receive_attempt(uint32_t queue_id,
                                 uint8_t task_id,
                                 uint8_t task_priority,
                                 uint32_t timeout_ticks) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_RECV_ATTEMPT %lu %u %u %lu\n",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned int)task_priority,
                      (unsigned long)timeout_ticks);
#endif
}

void trace_queue_receive_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_RECV_SUCCESS %lu %u %lu\n",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned long)message_hash);
#endif
}

void trace_queue_receive_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_RECV_BLOCK %lu %u %u\n",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned int)task_priority);
#endif
}

void trace_queue_receive_timeout(uint32_t queue_id, uint8_t task_id) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    SEGGER_RTT_printf(
        0, "QUEUE_RECV_TIMEOUT %lu %u\n", (unsigned long)queue_id, (unsigned int)task_id);
#endif
}

void trace_queue_wake_sender(uint32_t queue_id, uint8_t task_id) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    SEGGER_RTT_printf(
        0, "QUEUE_WAKE_SEND %lu %u\n", (unsigned long)queue_id, (unsigned int)task_id);
#endif
}

void trace_queue_wake_receiver(uint32_t queue_id, uint8_t task_id) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    SEGGER_RTT_printf(
        0, "QUEUE_WAKE_RECV %lu %u\n", (unsigned long)queue_id, (unsigned int)task_id);
#endif
}

void trace_queue_handoff(uint32_t queue_id,
                         uint8_t sender_id,
                         uint8_t receiver_id,
                         uint32_t message_hash) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_HANDOFF %lu %u %u %lu\n",
                      (unsigned long)queue_id,
                      (unsigned int)sender_id,
                      (unsigned int)receiver_id,
                      (unsigned long)message_hash);
#endif
}

void trace_queue_fill(uint32_t queue_id, uint32_t fill) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_emit("QUEUE_FILL %lu %lu\n", (unsigned long)queue_id, (unsigned long)fill);
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
