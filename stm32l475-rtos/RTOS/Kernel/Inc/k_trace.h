/**
 * @file k_trace.h
 * @brief Kernel-to-trace adaptation helpers.
 * @author Jerome
 *
 * @details
 * This header is the only adaptation boundary that knows both the internal
 * @ref kernel_task_t representation and the backend-independent trace task
 * types. It converts kernel task objects to small value objects before they
 * cross into the trace subsystem.
 *
 * Keeping these conversions on the kernel side preserves the dependency
 * direction Kernel -> Trace. The trace implementation never includes
 * @c kernel_task.h or @c tcb.h and therefore cannot query or mutate scheduler
 * state.
 */

#ifndef K_TRACE_H_
#define K_TRACE_H_

#include "kernel_task.h"
#include "trace.h"

#include <stdint.h>

/**
 * @brief Convert an optional kernel task to a backend-independent trace ref.
 *
 * @param task Kernel task to convert, or 0 when no task is associated with the
 *             event (for example exception-context semaphore operations).
 *
 * @return Task ID/priority pair, or @ref trace_task_ref_none when @p task is 0.
 */
static inline trace_task_ref_t k_trace_task_ref(const kernel_task_t *task) {
    if (task == 0) {
        return trace_task_ref_none();
    }

    return trace_task_ref(task->tcb.id, task->tcb.priority);
}

/**
 * @brief Build the metadata snapshot cached when a task is created.
 *
 * @param task Fully initialized kernel task.
 * @param kind Classification of the task as normal or idle.
 *
 * @return Trace-owned metadata containing no pointer that the trace subsystem
 *         needs to dereference.
 *
 * @pre @p task must not be 0.
 * @pre The TCB ID, priority, stack allocation, and saved stack pointer must
 *      already have been initialized.
 *
 * @note @ref runtime_id intentionally stores the TCB address only as an opaque
 *       numeric identity. SystemView requires a stable RAM-based task ID when a
 *       nonzero RAM base is configured; the trace subsystem never dereferences
 *       this value.
 */
static inline trace_task_info_t k_trace_task_info(const kernel_task_t *task,
                                                  trace_task_kind_t kind) {
    trace_task_info_t info = {
        .task = trace_task_ref_none(),
        .runtime_id = 0u,
        .stack_base = 0u,
        .stack_size = 0u,
        .kind = kind,
    };

    if (task == 0) {
        return info;
    }

    info.task = k_trace_task_ref(task);
    info.runtime_id = (uintptr_t)&task->tcb;
    info.stack_base = (uintptr_t)task->tcb.stack;
    info.stack_size = (uint32_t)sizeof(task->tcb.stack);

    return info;
}

#endif /* K_TRACE_H_ */
