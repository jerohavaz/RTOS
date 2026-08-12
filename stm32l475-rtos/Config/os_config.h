#ifndef OS_CONFIG_H_
#define OS_CONFIG_H_

#include <stdbool.h>

// Tasks
#define OS_MAX_TASKS       (3u)
#define OS_TASK_STACK_SIZE (512u)

// Priorities
#define OS_MAX_PRIORITIES        (32u)
#define OS_TASK_PRIORITY_LOWEST  (0u)
#define OS_TASK_PRIORITY_HIGHEST (OS_MAX_PRIORITIES - 1u)

// Tracing master switch
#define OS_TRACE_ENABLED (true)

// Trace backends
#define OS_TRACE_SEGGER_SYSVIEW (false)
#define OS_TRACE_TESSLA_RTT     (true)

// Trace categories
#define OS_TRACE_SCHEDULER (false)
#define OS_TRACE_TASKS     (false)
#define OS_TRACE_ISR       (false)
#define OS_TRACE_MUTEX     (false)
#define OS_TRACE_SEMAPHORE (false)
#define OS_TRACE_QUEUE     (false)
#define OS_TRACE_DELAY     (false)
#define OS_TRACE_PROJECT   (true)

// Cortex-M interrupt priorities
#define OS_SVC_INTERRUPT_PRIORITY     (13u)
#define OS_SYSTICK_INTERRUPT_PRIORITY (14u)
#define OS_PENDSV_INTERRUPT_PRIORITY  (15u)

#define OS_KERNEL_INTERRUPT_PRIORITY OS_SVC_INTERRUPT_PRIORITY

#endif /* OS_CONFIG_H_ */