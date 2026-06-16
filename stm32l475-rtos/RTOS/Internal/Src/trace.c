#include "trace.h"
#include "port.h"

#if TRACE_ENABLED

#include "SEGGER_RTT.h"
#include "SEGGER_SYSVIEW.h"
#include "stm32l4xx.h"

void trace_init(void) {
    SEGGER_SYSVIEW_Conf();
    SEGGER_SYSVIEW_Print("SYSVIEW started");
}

static U32 sv_task_id(TCB_sctTCB_t *task) {
    return (U32)(uintptr_t)task;
}

void trace_task_create(TCB_sctTCB_t *task) {
    SEGGER_SYSVIEW_OnTaskCreate(sv_task_id(task));
}

void trace_task_ready(TCB_sctTCB_t *task) {
    SEGGER_SYSVIEW_OnTaskStartReady(sv_task_id(task));
}

void trace_task_run(TCB_sctTCB_t *task) {
    SEGGER_SYSVIEW_OnTaskStartExec(sv_task_id(task));
}

void trace_task_stop_run(void) {
    SEGGER_SYSVIEW_OnTaskStopExec();
}

void trace_task_block(TCB_sctTCB_t *task) {
    SEGGER_SYSVIEW_OnTaskStopReady(sv_task_id(task), 0);
}

void trace_isr_enter(void) {
    SEGGER_SYSVIEW_RecordEnterISR();
}

void trace_isr_exit(void) {
    SEGGER_SYSVIEW_RecordExitISR();
}

void trace_isr_exit_to_scheduler(void) {
    SEGGER_SYSVIEW_RecordExitISRToScheduler();
}

void trace_log(const char *text) {
    SEGGER_SYSVIEW_Print(text);
}

long unsigned int SEGGER_SYSVIEW_X_GetInterruptId(void) {
    return port_get_active_exception_id();
}

#endif