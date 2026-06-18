#ifndef OS_CONFIG_H_
#define OS_CONFIG_H_

#define OS_TASK_STACK_SIZE (512u)

#define OS_MAX_TASKS (3u)

#define OS_MAX_PRIORITIES        (32u)
#define OS_TASK_PRIORITY_LOWEST  0u
#define OS_TASK_PRIORITY_HIGHEST (OS_MAX_PRIORITIES - 1u)

#define OS_TRACE_ENABLED (1u)

#endif