#ifndef OS_CONFIG_H_
#define OS_CONFIG_H_

// Tasks
#define OS_MAX_TASKS       (3u) // TODO: make this independent of idle
#define OS_TASK_STACK_SIZE (512u)

// Priorities
#define OS_MAX_PRIORITIES        (32u)
#define OS_TASK_PRIORITY_LOWEST  (0u)
#define OS_TASK_PRIORITY_HIGHEST (OS_MAX_PRIORITIES - 1u)

// Tracing master switch
#define OS_TRACE_ENABLED (1u)

// Trace backends
#define OS_TRACE_SEGGER_SYSVIEW (1u)
#define OS_TRACE_TESSLA_RTT     (1u)

// Trace categories
#define OS_TRACE_SCHEDULER (1u)
#define OS_TRACE_TASKS     (1u)
#define OS_TRACE_ISR       (1u)
#define OS_TRACE_MUTEX     (0u)
#define OS_TRACE_SEMAPHORE (0u)
#define OS_TRACE_QUEUE     (0u)
#define OS_TRACE_DELAY     (1u)

// Cortex-M interrupt priorities
#define OS_SVC_INTERRUPT_PRIORITY     (13u)
#define OS_SYSTICK_INTERRUPT_PRIORITY (14u)
#define OS_PENDSV_INTERRUPT_PRIORITY  (15u)

#define OS_KERNEL_INTERRUPT_PRIORITY OS_SVC_INTERRUPT_PRIORITY

#endif /* OS_CONFIG_H_ */