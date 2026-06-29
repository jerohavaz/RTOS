#ifndef OS_CONFIG_H_
#define OS_CONFIG_H_

// Tasks
#define OS_MAX_TASKS       (6u) // TODO: make this independent of idle
#define OS_TASK_STACK_SIZE (512u)

// Priorities
#define OS_MAX_PRIORITIES        (32u)
#define OS_TASK_PRIORITY_LOWEST  (0u)
#define OS_TASK_PRIORITY_HIGHEST (OS_MAX_PRIORITIES - 1u)

// Tracing
#define OS_TRACE_ENABLED (1u)

// Cortex-M interrupt priorities
#define OS_SVC_INTERRUPT_PRIORITY     (13u)
#define OS_SYSTICK_INTERRUPT_PRIORITY (14u)
#define OS_PENDSV_INTERRUPT_PRIORITY  (15u)

#define OS_KERNEL_INTERRUPT_PRIORITY OS_SVC_INTERRUPT_PRIORITY

#endif /* OS_CONFIG_H */