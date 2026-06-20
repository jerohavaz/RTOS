#ifndef OS_TYPES_H_
#define OS_TYPES_H_

typedef enum {
    OS_OK = 0,
    OS_ERR_NULL,
    OS_ERR_FULL,
    OS_ERR_INVALID_PRIO,
    OS_ERR_INVALID_STATE,
    OS_ERR_TIMEOUT
} os_status_t;

#endif