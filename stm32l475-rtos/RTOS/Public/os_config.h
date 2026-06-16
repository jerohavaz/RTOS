#ifndef OS_CONFIG_H_
#define OS_CONFIG_H_

#define OS_TASK_STACK_SIZE    (512u)

#define OS_MAX_TASKS          (3u)
#define OS_MAX_PRIORITIES     (8u)

#define OS_IDLE_TASK_PRIORITY (0u)
#define OS_USER_MIN_PRIORITY  (1u)
#define OS_USER_MAX_PRIORITY  (OS_MAX_PRIORITIES - 1u)

#endif