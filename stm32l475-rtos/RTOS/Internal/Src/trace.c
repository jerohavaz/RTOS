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
 * Message queue events
 * -------------------------------------------------------------------------- */

void trace_queue_create(uint32_t queue_id, uint32_t capacity) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    SEGGER_RTT_printf(
        0, "QUEUE_CREATE %lu %lu\n", (unsigned long)queue_id, (unsigned long)capacity);
#endif
}

void trace_queue_send_attempt(uint32_t queue_id,
                              uint8_t task_id,
                              uint8_t task_priority,
                              uint32_t timeout_ticks,
                              uint32_t message_hash) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    SEGGER_RTT_printf(0,
                      "QUEUE_SEND_ATTEMPT %lu %u %u %lu %lu\n",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned int)task_priority,
                      (unsigned long)timeout_ticks,
                      (unsigned long)message_hash);
#endif
}

void trace_queue_send_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    SEGGER_RTT_printf(0,
                      "QUEUE_SEND_SUCCESS %lu %u %lu\n",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned long)message_hash);
#endif
}

void trace_queue_send_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    SEGGER_RTT_printf(0,
                      "QUEUE_SEND_BLOCK %lu %u %u\n",
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
    SEGGER_RTT_printf(0,
                      "QUEUE_RECV_ATTEMPT %lu %u %u %lu\n",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned int)task_priority,
                      (unsigned long)timeout_ticks);
#endif
}

void trace_queue_receive_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    SEGGER_RTT_printf(0,
                      "QUEUE_RECV_SUCCESS %lu %u %lu\n",
                      (unsigned long)queue_id,
                      (unsigned int)task_id,
                      (unsigned long)message_hash);
#endif
}

void trace_queue_receive_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    SEGGER_RTT_printf(0,
                      "QUEUE_RECV_BLOCK %lu %u %u\n",
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
    SEGGER_RTT_printf(0,
                      "QUEUE_HANDOFF %lu %u %u %lu\n",
                      (unsigned long)queue_id,
                      (unsigned int)sender_id,
                      (unsigned int)receiver_id,
                      (unsigned long)message_hash);
#endif
}

void trace_queue_fill(uint32_t queue_id, uint32_t fill) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    SEGGER_RTT_printf(0, "QUEUE_FILL %lu %lu\n", (unsigned long)queue_id, (unsigned long)fill);
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

#if OS_TRACE_SEGGER_SYSVIEW
unsigned long SEGGER_SYSVIEW_X_GetInterruptId(void) {
    return (unsigned long)port_get_active_exception_id();
}
#endif

#endif /* OS_TRACE_ENABLED */