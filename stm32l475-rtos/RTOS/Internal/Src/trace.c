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
    SEGGER_RTT_WriteString(0, "TESSLA_START\n");
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
    SEGGER_RTT_printf(
        0, "TASK_CREATE %u %u\n", (unsigned int)task->u8TaskId, (unsigned int)task->u8TaskPrio);
#endif
}

void trace_task_state(uint8_t task_id, uint8_t old_state, uint8_t new_state) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_TASKS
    SEGGER_RTT_printf(0,
                      "STATE %u %u %u\n",
                      (unsigned int)task_id,
                      (unsigned int)old_state,
                      (unsigned int)new_state);
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
    SEGGER_RTT_printf(
        0, "READY %u %u\n", (unsigned int)task->u8TaskId, (unsigned int)task->u8TaskPrio);
#endif
}

void trace_task_run(TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    SEGGER_SYSVIEW_OnTaskStartExec(sv_task_id(task));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    SEGGER_RTT_printf(
        0, "RUNNING %u %u\n", (unsigned int)task->u8TaskId, (unsigned int)task->u8TaskPrio);
#endif
}

void trace_task_stop_run(void) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    SEGGER_SYSVIEW_OnTaskStopExec();
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    SEGGER_RTT_WriteString(0, "STOP_RUNNING\n");
#endif
}

void trace_task_block(TCB_sctTCB_t *task) {
    KERNEL_REQUIRE(task != 0);

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    SEGGER_SYSVIEW_OnTaskStopReady(sv_task_id(task), 0u);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    SEGGER_RTT_printf(0, "BLOCKED %u\n", (unsigned int)task->u8TaskId);
#endif
}

void trace_idle(void) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    SEGGER_SYSVIEW_OnIdle();
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    SEGGER_RTT_WriteString(0, "IDLE\n");
#endif
}

void trace_tick(uint32_t dt) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    SEGGER_RTT_printf(0, "TICK %lu\n", (unsigned long)dt);
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
        SEGGER_RTT_WriteString(0, text);
        SEGGER_RTT_WriteString(0, "\n");
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
unsigned long SEGGER_SYSVIEW_X_GetInterruptId(void) {
    return (unsigned long)port_get_active_exception_id();
}
#endif

#endif /* OS_TRACE_ENABLED */