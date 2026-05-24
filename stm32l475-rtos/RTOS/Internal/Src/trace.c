#include "trace.h"
#include "port.h"

#if TRACE_ENABLED

#include "SEGGER_RTT.h"
#include "SEGGER_SYSVIEW.h"
#include "stm32l4xx.h"

void trace_init(void) {
    // SEGGER_RTT_Init();
    SEGGER_SYSVIEW_Conf();
    SEGGER_SYSVIEW_Start();
    SEGGER_SYSVIEW_Print("SYSVIEW started");
}

void trace_task_create(tcb_t *task) {
    SEGGER_SYSVIEW_OnTaskCreate((U32)task->id);
}

void trace_task_ready(tcb_t *task) {
    SEGGER_SYSVIEW_OnTaskStartReady((U32)task->id);
}

void trace_task_run(tcb_t *task) {
    SEGGER_SYSVIEW_OnTaskStartExec((U32)task->id);
}

void trace_task_stop_run(void) {
    SEGGER_SYSVIEW_OnTaskStopExec();
}

void trace_task_block(tcb_t *task) {
    SEGGER_SYSVIEW_OnTaskStopReady((U32)task->id, 0);
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