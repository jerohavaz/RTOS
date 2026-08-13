/**
 * @file trace.c
 * @brief Backend-independent RTOS trace implementation and task registry.
 * @author Jerome
 *
 * @details
 * Owns the trace-side snapshot of task metadata and routes kernel events to
 * SEGGER SystemView and the TeSSLa RTT stream. This module deliberately has no
 * dependency on kernel task tables, scheduler objects, or tcb.h. SystemView's
 * task-list callback replays the metadata previously supplied through
 * trace_task_register().
 */

#include "trace.h"

#if OS_TRACE_ENABLED

#include "port.h"

#if OS_TRACE_SEGGER_SYSVIEW || OS_TRACE_TESSLA_RTT
#include "SEGGER_RTT.h"
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
#include <stddef.h>
#include <string.h>

#define TRACE_TESSLA_RTT_CHANNEL       (0u)
#define TRACE_TESSLA_HEADER_SIZE       (4u)
#define TRACE_TESSLA_MAX_PAYLOAD_SIZE  (96u)
#define TRACE_TESSLA_FIXED_PAYLOAD_SIZE (20u)

typedef enum {
    TRACE_TESSLA_EVT_SESSION_START = 0u,
    TRACE_TESSLA_EVT_TASK_CREATE = 1u,
    TRACE_TESSLA_EVT_STATE = 2u,
    TRACE_TESSLA_EVT_READY = 3u,
    TRACE_TESSLA_EVT_RUNNING = 4u,
    TRACE_TESSLA_EVT_STOP_RUNNING = 5u,
    TRACE_TESSLA_EVT_BLOCKED = 6u,
    TRACE_TESSLA_EVT_IDLE = 7u,
    TRACE_TESSLA_EVT_TICK = 8u,
    TRACE_TESSLA_EVT_DELAY_BUSY_START = 9u,
    TRACE_TESSLA_EVT_DELAY_BUSY_END = 10u,
    TRACE_TESSLA_EVT_DELAY_START = 11u,
    TRACE_TESSLA_EVT_DELAY_END = 12u,
    TRACE_TESSLA_EVT_SEM_CREATE = 13u,
    TRACE_TESSLA_EVT_SEM_ACQUIRE_ENTER = 14u,
    TRACE_TESSLA_EVT_SEM_ACQUIRE_EXIT = 15u,
    TRACE_TESSLA_EVT_SEM_BLOCK = 16u,
    TRACE_TESSLA_EVT_SEM_TIMEOUT = 17u,
    TRACE_TESSLA_EVT_SEM_RELEASE = 18u,
    TRACE_TESSLA_EVT_SEM_WAKE = 19u,
    TRACE_TESSLA_EVT_MUTEX_CREATE = 20u,
    TRACE_TESSLA_EVT_MUTEX_LOCK_ENTER = 21u,
    TRACE_TESSLA_EVT_MUTEX_LOCK_EXIT = 22u,
    TRACE_TESSLA_EVT_MUTEX_BLOCK = 23u,
    TRACE_TESSLA_EVT_MUTEX_TIMEOUT = 24u,
    TRACE_TESSLA_EVT_MUTEX_UNLOCK = 25u,
    TRACE_TESSLA_EVT_MUTEX_WAKE = 26u,
    TRACE_TESSLA_EVT_QUEUE_CREATE = 27u,
    TRACE_TESSLA_EVT_QUEUE_SEND_ATTEMPT = 28u,
    TRACE_TESSLA_EVT_QUEUE_SEND_SUCCESS = 29u,
    TRACE_TESSLA_EVT_QUEUE_SEND_BLOCK = 30u,
    TRACE_TESSLA_EVT_QUEUE_SEND_TIMEOUT = 31u,
    TRACE_TESSLA_EVT_QUEUE_RECV_ATTEMPT = 32u,
    TRACE_TESSLA_EVT_QUEUE_RECV_SUCCESS = 33u,
    TRACE_TESSLA_EVT_QUEUE_RECV_BLOCK = 34u,
    TRACE_TESSLA_EVT_QUEUE_RECV_TIMEOUT = 35u,
    TRACE_TESSLA_EVT_QUEUE_WAKE_SEND = 36u,
    TRACE_TESSLA_EVT_QUEUE_WAKE_RECV = 37u,
    TRACE_TESSLA_EVT_QUEUE_HANDOFF = 38u,
    TRACE_TESSLA_EVT_QUEUE_FILL = 39u,
    TRACE_TESSLA_EVT_TRANSMISSION_COMPLETE = 40u,
    TRACE_TESSLA_EVT_LOG = 41u
} trace_tessla_event_id_t;

typedef struct {
    uint8_t bytes[TRACE_TESSLA_FIXED_PAYLOAD_SIZE];
    uint8_t length;
} trace_tessla_payload_t;

static uint16_t g_trace_sequence;

static void trace_tessla_put_u8(trace_tessla_payload_t *payload, uint8_t value) {
    payload->bytes[payload->length++] = value;
}

static void trace_tessla_put_u32(trace_tessla_payload_t *payload, uint32_t value) {
    uint8_t *destination = &payload->bytes[payload->length];

    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8u);
    destination[2] = (uint8_t)(value >> 16u);
    destination[3] = (uint8_t)(value >> 24u);
    payload->length += 4u;
}

/**
 * @brief Submit one binary TeSSLa event as an RTT record.
 *
 * Sequence allocation and RTT insertion occur within the same critical
 * section, preventing task and SysTick producers from appearing out of order.
 *
 * RTT remains non-blocking. If the complete record does not fit, it is
 * discarded. Because its sequence number has already been consumed, the
 * receiver detects the loss when the next record arrives.
 *
 * @param event_id Numeric event identifier.
 * @param payload Binary event fields in protocol order, or null when empty.
 * @param payload_length Number of payload bytes.
 */
static void trace_tessla_emit(trace_tessla_event_id_t event_id,
                              const uint8_t *payload,
                              uint8_t payload_length) {
    uint8_t record[TRACE_TESSLA_HEADER_SIZE + TRACE_TESSLA_MAX_PAYLOAD_SIZE];

    if (payload_length > TRACE_TESSLA_MAX_PAYLOAD_SIZE ||
        (payload_length != 0u && payload == 0)) {
        return;
    }

    record[2] = (uint8_t)event_id;
    record[3] = payload_length;

    if (payload_length != 0u) {
        memcpy(&record[TRACE_TESSLA_HEADER_SIZE], payload, payload_length);
    }

    uint32_t key = port_enter_critical();
    uint16_t sequence = g_trace_sequence++;

    record[0] = (uint8_t)sequence;
    record[1] = (uint8_t)(sequence >> 8u);
    SEGGER_RTT_WriteSkipNoLock(TRACE_TESSLA_RTT_CHANNEL,
                               record,
                               TRACE_TESSLA_HEADER_SIZE + (unsigned int)payload_length);

    port_exit_critical(key);
}
#endif

#define TRACE_TASK_REGISTRY_CAPACITY (OS_MAX_TASKS + 1u)

static trace_task_info_t g_trace_tasks[TRACE_TASK_REGISTRY_CAPACITY];
static uint32_t g_trace_task_count;

/**
 * @brief Find cached task metadata by numeric RTOS task ID.
 *
 * @param task_id Numeric task ID to look up.
 *
 * @return Pointer to the cached metadata, or 0 when the task has not been
 *         registered with the trace subsystem.
 */
static const trace_task_info_t *trace_task_lookup(uint8_t task_id) {
    for (uint32_t i = 0u; i < g_trace_task_count; ++i) {
        if (g_trace_tasks[i].task.id == task_id) {
            return &g_trace_tasks[i];
        }
    }

    return 0;
}

/**
 * @brief Check whether a task reference represents a real RTOS task.
 *
 * @param task Task reference to inspect.
 * @retval 1 The reference contains a real task ID.
 * @retval 0 The reference is the no-task sentinel.
 */
static inline uint8_t trace_task_ref_valid(trace_task_ref_t task) {
    return (uint8_t)(task.id != TRACE_TASK_ID_NONE);
}

#if OS_TRACE_SEGGER_SYSVIEW

/**
 * @brief Send one cached normal task to SEGGER SystemView.
 *
 * @param task Cached task metadata to report.
 *
 * @pre @p task must not be 0.
 * @pre @p task must describe a normal, not idle, task.
 *
 * @note SEGGER_SYSVIEW_SendTaskInfo() encodes the task name immediately, so
 *       the local name buffer does not need to outlive this function.
 */
static void sv_send_task_info(const trace_task_info_t *task) {
    if ((task == 0) || (task->kind != TRACE_TASK_KIND_NORMAL) || (task->runtime_id == 0u)) {
        return;
    }

    char name[4]; /* "255" plus '\0' */
    uint8_t id = task->task.id;
    uint8_t index = 0u;

    if (id >= 100u) {
        name[index++] = (char)('0' + (id / 100u));
        id %= 100u;
        name[index++] = (char)('0' + (id / 10u));
    } else if (id >= 10u) {
        name[index++] = (char)('0' + (id / 10u));
    }

    name[index++] = (char)('0' + (id % 10u));
    name[index] = '\0';

    const SEGGER_SYSVIEW_TASKINFO info = {
        .TaskID = (U32)task->runtime_id,
        .sName = name,
        .Prio = (U32)task->task.priority,
        .StackBase = (U32)task->stack_base,
        .StackSize = (U32)task->stack_size,
        .StackUsage = 0u,
    };

    SEGGER_SYSVIEW_SendTaskInfo(&info);
}

/**
 * @brief Replay all currently registered normal tasks to SystemView.
 *
 * SystemView invokes this callback when a recording session requests the
 * current task list. The information comes entirely from the trace-owned
 * registry populated by trace_task_register(); no kernel task table is queried.
 */
static void sv_send_task_list(void) {
    for (uint32_t i = 0u; i < g_trace_task_count; ++i) {
        if (g_trace_tasks[i].kind == TRACE_TASK_KIND_NORMAL) {
            sv_send_task_info(&g_trace_tasks[i]);
        }
    }
}

/**
 * @brief SystemView OS integration used by SEGGER_SYSVIEW_Conf().
 *
 * A custom absolute-time callback is unnecessary because SystemView already
 * timestamps events with the configured DWT cycle counter. The task-list
 * callback is required so task metadata can be resent when recording starts
 * after task creation.
 */
const SEGGER_SYSVIEW_OS_API g_trace_sysview_os_api = {
    .pfGetTime = 0,
    .pfSendTaskList = sv_send_task_list,
};

/**
 * @brief Resolve a numeric RTOS task ID to the cached SystemView task ID.
 *
 * @param task_id Numeric RTOS task ID.
 * @param[out] systemview_id Receives the opaque SystemView task identity.
 *
 * @retval 1 The task was registered and has a valid runtime identity.
 * @retval 0 The task is unknown, idle, or has no runtime identity.
 */
static uint8_t sv_resolve_task_id(uint8_t task_id, U32 *systemview_id) {
    const trace_task_info_t *task = trace_task_lookup(task_id);

    if ((task == 0) || (task->kind != TRACE_TASK_KIND_NORMAL) || (task->runtime_id == 0u) ||
        (systemview_id == 0)) {
        return 0u;
    }

    *systemview_id = (U32)task->runtime_id;
    return 1u;
}
#endif

void trace_init(void) {
    g_trace_task_count = 0u;

#if OS_TRACE_TESSLA_RTT
    g_trace_sequence = 0u;
    SEGGER_RTT_Init();
    trace_tessla_emit(TRACE_TESSLA_EVT_SESSION_START, 0, 0u);
#endif

#if OS_TRACE_SEGGER_SYSVIEW
    SEGGER_SYSVIEW_Conf();
    SEGGER_SYSVIEW_Start();
#endif
}

/* --------------------------------------------------------------------------
 * Task events
 * -------------------------------------------------------------------------- */

void trace_task_register(const trace_task_info_t *info) {
    if ((info == 0) || (info->task.id == TRACE_TASK_ID_NONE)) {
        return;
    }

    if (trace_task_lookup(info->task.id) != 0) {
        return;
    }

    if (g_trace_task_count >= TRACE_TASK_REGISTRY_CAPACITY) {
        return;
    }

    g_trace_tasks[g_trace_task_count++] = *info;

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_TASKS
    if (info->kind == TRACE_TASK_KIND_NORMAL && info->runtime_id != 0u) {
        SEGGER_SYSVIEW_OnTaskCreate((U32)info->runtime_id);
        sv_send_task_info(info);
    }
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_TASKS
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u8(&payload, info->task.id);
    trace_tessla_put_u8(&payload, info->task.priority);
    trace_tessla_emit(TRACE_TESSLA_EVT_TASK_CREATE, payload.bytes, payload.length);
#endif
}

void trace_task_state(uint8_t task_id, uint8_t old_state, uint8_t new_state) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_TASKS
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u8(&payload, task_id);
    trace_tessla_put_u8(&payload, old_state);
    trace_tessla_put_u8(&payload, new_state);
    trace_tessla_emit(TRACE_TESSLA_EVT_STATE, payload.bytes, payload.length);
#endif
}

/* --------------------------------------------------------------------------
 * Scheduler events
 * -------------------------------------------------------------------------- */

void trace_task_ready(trace_task_ref_t task) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    U32 systemview_id;

    if (sv_resolve_task_id(task.id, &systemview_id) != 0u) {
        SEGGER_SYSVIEW_OnTaskStartReady(systemview_id);
    }
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    if (trace_task_ref_valid(task) != 0u) {
        trace_tessla_payload_t payload = { 0 };
        trace_tessla_put_u8(&payload, task.id);
        trace_tessla_put_u8(&payload, task.priority);
        trace_tessla_emit(TRACE_TESSLA_EVT_READY, payload.bytes, payload.length);
    }
#endif
}

void trace_task_run(trace_task_ref_t task) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    U32 systemview_id;

    if (sv_resolve_task_id(task.id, &systemview_id) != 0u) {
        SEGGER_SYSVIEW_OnTaskStartExec(systemview_id);
    }
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    if (trace_task_ref_valid(task) != 0u) {
        trace_tessla_payload_t payload = { 0 };
        trace_tessla_put_u8(&payload, task.id);
        trace_tessla_put_u8(&payload, task.priority);
        trace_tessla_emit(TRACE_TESSLA_EVT_RUNNING, payload.bytes, payload.length);
    }
#endif
}

void trace_task_stop_run(void) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    SEGGER_SYSVIEW_OnTaskStopExec();
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    trace_tessla_emit(TRACE_TESSLA_EVT_STOP_RUNNING, 0, 0u);
#endif
}

void trace_task_block(trace_task_ref_t task) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    U32 systemview_id;

    if (sv_resolve_task_id(task.id, &systemview_id) != 0u) {
        SEGGER_SYSVIEW_OnTaskStopReady(systemview_id, 0u);
    }
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    if (trace_task_ref_valid(task) != 0u) {
        trace_tessla_emit(TRACE_TESSLA_EVT_BLOCKED, &task.id, 1u);
    }
#endif
}

void trace_idle(void) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SCHEDULER
    SEGGER_SYSVIEW_OnIdle();
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    trace_tessla_emit(TRACE_TESSLA_EVT_IDLE, 0, 0u);
#endif
}

void trace_tick(uint32_t dt) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_SCHEDULER
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, dt);
    trace_tessla_emit(TRACE_TESSLA_EVT_TICK, payload.bytes, payload.length);
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

void trace_task_delay_busy_start(trace_task_ref_t task, uint32_t delay_ticks) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_DELAY
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_DELAY_BUSY_START, (U32)task.id, (U32)delay_ticks);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_DELAY
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u32(&payload, delay_ticks);
    trace_tessla_emit(TRACE_TESSLA_EVT_DELAY_BUSY_START, payload.bytes, payload.length);
#endif
}

void trace_task_delay_busy_end(trace_task_ref_t task) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_DELAY
    SEGGER_SYSVIEW_RecordU32(TRACE_SV_EVT_DELAY_BUSY_END, (U32)task.id);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_DELAY
    trace_tessla_emit(TRACE_TESSLA_EVT_DELAY_BUSY_END, &task.id, 1u);
#endif
}

void trace_task_delay_start(trace_task_ref_t task, uint32_t delay_ticks) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_DELAY
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_DELAY_START, (U32)task.id, (U32)delay_ticks);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_DELAY
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u32(&payload, delay_ticks);
    trace_tessla_emit(TRACE_TESSLA_EVT_DELAY_START, payload.bytes, payload.length);
#endif
}

void trace_task_delay_end(trace_task_ref_t task) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_DELAY
    SEGGER_SYSVIEW_RecordU32(TRACE_SV_EVT_DELAY_END, (U32)task.id);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_DELAY
    trace_tessla_emit(TRACE_TESSLA_EVT_DELAY_END, &task.id, 1u);
#endif
}

/* --------------------------------------------------------------------------
 * Counting-semaphore events
 * -------------------------------------------------------------------------- */

#if (OS_TRACE_SEGGER_SYSVIEW || OS_TRACE_TESSLA_RTT) && OS_TRACE_SEMAPHORE
/**
 * @brief Convert a semaphore address to its numeric trace identifier.
 *
 * @param semaphore Semaphore object whose stable address is required.
 * @return Address of @p semaphore represented as an unsigned integer.
 */
static uint32_t trace_sem_id(const void *semaphore) {
    return (semaphore != 0) ? (uint32_t)(uintptr_t)semaphore : 0u;
}
#endif

void trace_sem_create(const void *semaphore, uint32_t initial_count, uint32_t max_count) {
    if (semaphore == 0) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_SEM_CREATE, (U32)trace_sem_id(semaphore), (U32)initial_count, (U32)max_count);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_sem_id(semaphore));
    trace_tessla_put_u32(&payload, initial_count);
    trace_tessla_put_u32(&payload, max_count);
    trace_tessla_emit(TRACE_TESSLA_EVT_SEM_CREATE, payload.bytes, payload.length);
#endif
}

void trace_sem_acquire_enter(const void *semaphore,
                             trace_task_ref_t task,
                             uint32_t count,
                             uint32_t timeout_ticks,
                             uint8_t finite_timeout) {
    if (semaphore == 0) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x5(TRACE_SV_EVT_SEM_ACQUIRE_ENTER,
                               (U32)trace_sem_id(semaphore),
                               (U32)task.id,
                               (U32)count,
                               (U32)timeout_ticks,
                               (U32)(finite_timeout != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_sem_id(semaphore));
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u32(&payload, count);
    trace_tessla_put_u32(&payload, timeout_ticks);
    trace_tessla_put_u8(&payload, (uint8_t)(finite_timeout != 0u));
    trace_tessla_emit(TRACE_TESSLA_EVT_SEM_ACQUIRE_ENTER, payload.bytes, payload.length);
#endif
}

void trace_sem_acquire_exit(const void *semaphore,
                            trace_task_ref_t task,
                            uint32_t count,
                            uint8_t succeeded) {
    if (semaphore == 0) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x4(TRACE_SV_EVT_SEM_ACQUIRE_EXIT,
                               (U32)trace_sem_id(semaphore),
                               (U32)task.id,
                               (U32)count,
                               (U32)(succeeded != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_sem_id(semaphore));
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u32(&payload, count);
    trace_tessla_put_u8(&payload, (uint8_t)(succeeded != 0u));
    trace_tessla_emit(TRACE_TESSLA_EVT_SEM_ACQUIRE_EXIT, payload.bytes, payload.length);
#endif
}

void trace_sem_block(const void *semaphore,
                     trace_task_ref_t task,
                     uint32_t timeout_ticks,
                     uint8_t finite_timeout) {
    if ((semaphore == 0) || (trace_task_ref_valid(task) == 0u)) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x5(TRACE_SV_EVT_SEM_BLOCK,
                               (U32)trace_sem_id(semaphore),
                               (U32)task.id,
                               (U32)task.priority,
                               (U32)timeout_ticks,
                               (U32)(finite_timeout != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_sem_id(semaphore));
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u8(&payload, task.priority);
    trace_tessla_put_u32(&payload, timeout_ticks);
    trace_tessla_put_u8(&payload, (uint8_t)(finite_timeout != 0u));
    trace_tessla_emit(TRACE_TESSLA_EVT_SEM_BLOCK, payload.bytes, payload.length);
#endif
}

void trace_sem_timeout(const void *semaphore, trace_task_ref_t task, uint32_t count) {
    if ((semaphore == 0) || (trace_task_ref_valid(task) == 0u)) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_SEM_TIMEOUT, (U32)trace_sem_id(semaphore), (U32)task.id, (U32)count);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_sem_id(semaphore));
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u32(&payload, count);
    trace_tessla_emit(TRACE_TESSLA_EVT_SEM_TIMEOUT, payload.bytes, payload.length);
#endif
}

void trace_sem_release(const void *semaphore,
                       uint32_t count_before,
                       uint32_t count_after,
                       uint32_t max_count,
                       uint8_t succeeded) {
    if (semaphore == 0) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x5(TRACE_SV_EVT_SEM_RELEASE,
                               (U32)trace_sem_id(semaphore),
                               (U32)count_before,
                               (U32)count_after,
                               (U32)max_count,
                               (U32)(succeeded != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_sem_id(semaphore));
    trace_tessla_put_u32(&payload, count_before);
    trace_tessla_put_u32(&payload, count_after);
    trace_tessla_put_u32(&payload, max_count);
    trace_tessla_put_u8(&payload, (uint8_t)(succeeded != 0u));
    trace_tessla_emit(TRACE_TESSLA_EVT_SEM_RELEASE, payload.bytes, payload.length);
#endif
}

void trace_sem_wake(const void *semaphore, trace_task_ref_t task) {
    if ((semaphore == 0) || (trace_task_ref_valid(task) == 0u)) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_SEMAPHORE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_SEM_WAKE, (U32)trace_sem_id(semaphore), (U32)task.id, (U32)task.priority);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_SEMAPHORE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_sem_id(semaphore));
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u8(&payload, task.priority);
    trace_tessla_emit(TRACE_TESSLA_EVT_SEM_WAKE, payload.bytes, payload.length);
#endif
}

/* --------------------------------------------------------------------------
 * Mutex events
 * -------------------------------------------------------------------------- */

#if (OS_TRACE_SEGGER_SYSVIEW || OS_TRACE_TESSLA_RTT) && OS_TRACE_MUTEX
/**
 * @brief Convert a mutex address to its numeric trace identifier.
 *
 * @param mutex Mutex object whose stable address is required.
 * @return Address of @p mutex represented as an unsigned integer.
 */
static uint32_t trace_mutex_id(const void *mutex) {
    return (mutex != 0) ? (uint32_t)(uintptr_t)mutex : 0u;
}
#endif

void trace_mutex_create(const void *mutex) {
    if (mutex == 0) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32(TRACE_SV_EVT_MUTEX_CREATE, (U32)trace_mutex_id(mutex));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_mutex_id(mutex));
    trace_tessla_emit(TRACE_TESSLA_EVT_MUTEX_CREATE, payload.bytes, payload.length);
#endif
}

void trace_mutex_lock_enter(const void *mutex,
                            trace_task_ref_t task,
                            trace_task_ref_t owner,
                            uint32_t timeout_ticks,
                            uint8_t finite_timeout) {
    if (mutex == 0) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32x5(TRACE_SV_EVT_MUTEX_LOCK_ENTER,
                               (U32)trace_mutex_id(mutex),
                               (U32)task.id,
                               (U32)owner.id,
                               (U32)timeout_ticks,
                               (U32)(finite_timeout != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_mutex_id(mutex));
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u8(&payload, owner.id);
    trace_tessla_put_u32(&payload, timeout_ticks);
    trace_tessla_put_u8(&payload, (uint8_t)(finite_timeout != 0u));
    trace_tessla_emit(TRACE_TESSLA_EVT_MUTEX_LOCK_ENTER, payload.bytes, payload.length);
#endif
}

void trace_mutex_lock_exit(const void *mutex,
                           trace_task_ref_t task,
                           trace_task_ref_t owner,
                           uint8_t succeeded) {
    if (mutex == 0) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32x4(TRACE_SV_EVT_MUTEX_LOCK_EXIT,
                               (U32)trace_mutex_id(mutex),
                               (U32)task.id,
                               (U32)owner.id,
                               (U32)(succeeded != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_mutex_id(mutex));
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u8(&payload, owner.id);
    trace_tessla_put_u8(&payload, (uint8_t)(succeeded != 0u));
    trace_tessla_emit(TRACE_TESSLA_EVT_MUTEX_LOCK_EXIT, payload.bytes, payload.length);
#endif
}

void trace_mutex_block(const void *mutex,
                       trace_task_ref_t task,
                       trace_task_ref_t owner,
                       uint32_t timeout_ticks,
                       uint8_t finite_timeout) {
    if ((mutex == 0) || (trace_task_ref_valid(task) == 0u)) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32x6(TRACE_SV_EVT_MUTEX_BLOCK,
                               (U32)trace_mutex_id(mutex),
                               (U32)task.id,
                               (U32)task.priority,
                               (U32)owner.id,
                               (U32)timeout_ticks,
                               (U32)(finite_timeout != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_mutex_id(mutex));
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u8(&payload, task.priority);
    trace_tessla_put_u8(&payload, owner.id);
    trace_tessla_put_u32(&payload, timeout_ticks);
    trace_tessla_put_u8(&payload, (uint8_t)(finite_timeout != 0u));
    trace_tessla_emit(TRACE_TESSLA_EVT_MUTEX_BLOCK, payload.bytes, payload.length);
#endif
}

void trace_mutex_timeout(const void *mutex, trace_task_ref_t task, trace_task_ref_t owner) {
    if ((mutex == 0) || (trace_task_ref_valid(task) == 0u)) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_MUTEX_TIMEOUT, (U32)trace_mutex_id(mutex), (U32)task.id, (U32)owner.id);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_mutex_id(mutex));
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u8(&payload, owner.id);
    trace_tessla_emit(TRACE_TESSLA_EVT_MUTEX_TIMEOUT, payload.bytes, payload.length);
#endif
}

void trace_mutex_unlock(const void *mutex,
                        trace_task_ref_t task,
                        trace_task_ref_t owner_before,
                        trace_task_ref_t owner_after,
                        uint8_t succeeded) {
    if (mutex == 0) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32x5(TRACE_SV_EVT_MUTEX_UNLOCK,
                               (U32)trace_mutex_id(mutex),
                               (U32)task.id,
                               (U32)owner_before.id,
                               (U32)owner_after.id,
                               (U32)(succeeded != 0u));
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_mutex_id(mutex));
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u8(&payload, owner_before.id);
    trace_tessla_put_u8(&payload, owner_after.id);
    trace_tessla_put_u8(&payload, (uint8_t)(succeeded != 0u));
    trace_tessla_emit(TRACE_TESSLA_EVT_MUTEX_UNLOCK, payload.bytes, payload.length);
#endif
}

void trace_mutex_wake(const void *mutex, trace_task_ref_t task) {
    if ((mutex == 0) || (trace_task_ref_valid(task) == 0u)) {
        return;
    }

#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_MUTEX
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_MUTEX_WAKE, (U32)trace_mutex_id(mutex), (U32)task.id, (U32)task.priority);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_MUTEX
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, trace_mutex_id(mutex));
    trace_tessla_put_u8(&payload, task.id);
    trace_tessla_put_u8(&payload, task.priority);
    trace_tessla_emit(TRACE_TESSLA_EVT_MUTEX_WAKE, payload.bytes, payload.length);
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
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u32(&payload, capacity);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_CREATE, payload.bytes, payload.length);
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
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u8(&payload, task_id);
    trace_tessla_put_u8(&payload, task_priority);
    trace_tessla_put_u32(&payload, timeout_ticks);
    trace_tessla_put_u32(&payload, message_hash);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_SEND_ATTEMPT, payload.bytes, payload.length);
#endif
}

void trace_queue_send_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_QUEUE_SEND_SUCCESS, (U32)queue_id, (U32)task_id, (U32)message_hash);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u8(&payload, task_id);
    trace_tessla_put_u32(&payload, message_hash);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_SEND_SUCCESS, payload.bytes, payload.length);
#endif
}

void trace_queue_send_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_QUEUE_SEND_BLOCK, (U32)queue_id, (U32)task_id, (U32)task_priority);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u8(&payload, task_id);
    trace_tessla_put_u8(&payload, task_priority);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_SEND_BLOCK, payload.bytes, payload.length);
#endif
}

void trace_queue_send_timeout(uint32_t queue_id, uint8_t task_id) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_QUEUE_SEND_TIMEOUT, (U32)queue_id, (U32)task_id);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u8(&payload, task_id);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_SEND_TIMEOUT, payload.bytes, payload.length);
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
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u8(&payload, task_id);
    trace_tessla_put_u8(&payload, task_priority);
    trace_tessla_put_u32(&payload, timeout_ticks);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_RECV_ATTEMPT, payload.bytes, payload.length);
#endif
}

void trace_queue_receive_success(uint32_t queue_id, uint8_t task_id, uint32_t message_hash) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_QUEUE_RECV_SUCCESS, (U32)queue_id, (U32)task_id, (U32)message_hash);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u8(&payload, task_id);
    trace_tessla_put_u32(&payload, message_hash);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_RECV_SUCCESS, payload.bytes, payload.length);
#endif
}

void trace_queue_receive_block(uint32_t queue_id, uint8_t task_id, uint8_t task_priority) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x3(
        TRACE_SV_EVT_QUEUE_RECV_BLOCK, (U32)queue_id, (U32)task_id, (U32)task_priority);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u8(&payload, task_id);
    trace_tessla_put_u8(&payload, task_priority);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_RECV_BLOCK, payload.bytes, payload.length);
#endif
}

void trace_queue_receive_timeout(uint32_t queue_id, uint8_t task_id) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_QUEUE_RECV_TIMEOUT, (U32)queue_id, (U32)task_id);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u8(&payload, task_id);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_RECV_TIMEOUT, payload.bytes, payload.length);
#endif
}

void trace_queue_wake_sender(uint32_t queue_id, uint8_t task_id) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_QUEUE_WAKE_SEND, (U32)queue_id, (U32)task_id);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u8(&payload, task_id);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_WAKE_SEND, payload.bytes, payload.length);
#endif
}

void trace_queue_wake_receiver(uint32_t queue_id, uint8_t task_id) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_QUEUE_WAKE_RECV, (U32)queue_id, (U32)task_id);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u8(&payload, task_id);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_WAKE_RECV, payload.bytes, payload.length);
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
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u8(&payload, sender_id);
    trace_tessla_put_u8(&payload, receiver_id);
    trace_tessla_put_u32(&payload, message_hash);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_HANDOFF, payload.bytes, payload.length);
#endif
}

void trace_queue_fill(uint32_t queue_id, uint32_t fill) {
#if OS_TRACE_SEGGER_SYSVIEW && OS_TRACE_QUEUE
    SEGGER_SYSVIEW_RecordU32x2(TRACE_SV_EVT_QUEUE_FILL, (U32)queue_id, (U32)fill);
#endif

#if OS_TRACE_TESSLA_RTT && OS_TRACE_QUEUE
    trace_tessla_payload_t payload = { 0 };
    trace_tessla_put_u32(&payload, queue_id);
    trace_tessla_put_u32(&payload, fill);
    trace_tessla_emit(TRACE_TESSLA_EVT_QUEUE_FILL, payload.bytes, payload.length);
#endif
}

/* --------------------------------------------------------------------------
 * Project: 3D-Gyro-Accelerometer events
 * -------------------------------------------------------------------------- */

void trace_transmission_complete(void) {
#if OS_TRACE_TESSLA_RTT && OS_TRACE_PROJECT
    trace_tessla_emit(TRACE_TESSLA_EVT_TRANSMISSION_COMPLETE, 0, 0u);
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
        size_t length = 0u;

        while (length < TRACE_TESSLA_MAX_PAYLOAD_SIZE && text[length] != '\0') {
            ++length;
        }

        trace_tessla_emit(TRACE_TESSLA_EVT_LOG, (const uint8_t *)text, (uint8_t)length);
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
