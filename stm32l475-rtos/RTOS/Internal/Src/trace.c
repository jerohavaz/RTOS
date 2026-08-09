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

/*
 * Format one logical TeSSLa event and submit it to RTT as one record.
 *
 * The sequence allocation and RTT insertion are kept in the same critical
 * section so task and SysTick producers cannot appear out of order. RTT stays
 * non-blocking: if the complete record does not fit, it is skipped. Since the
 * sequence number has already been consumed, the receiver detects the loss
 * when the next record arrives.
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
 * Delay events
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

void trace_task_delay_start(TCB_sctTCB_t *task, uint32_t delay_ticks) {
    KERNEL_REQUIRE(task != 0);
#if OS_TRACE_TESSLA_RTT && OS_TRACE_DELAY
    trace_tessla_emit("DELAY_START %u %u", (unsigned int)task->u8TaskId, (unsigned int)delay_ticks);
#endif
}

void trace_task_delay_end(TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);
#if OS_TRACE_TESSLA_RTT && OS_TRACE_DELAY
    trace_tessla_emit("DELAY_END %u", (unsigned int)task->u8TaskId);
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

/*
 * SystemView calls this function to determine the currently active
 * Cortex-M exception number.
 *
 * Keep it available whenever SystemView is enabled, even if OS_TRACE_ISR
 * disables explicit ISR events. The SystemView configuration may reference it.
 */
#if OS_TRACE_SEGGER_SYSVIEW
unsigned long SEGGER_SYSVIEW_X_GetInterruptId(void) {
    return (unsigned long)port_get_active_exception_id();
}
#endif

#endif /* OS_TRACE_ENABLED */