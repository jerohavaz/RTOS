#ifndef RTOS_TYPES_H_
#define RTOS_TYPES_H_

typedef enum {
    RTOS_OK = 0,
    RTOS_ERR_NULL,
    RTOS_ERR_FULL,
    RTOS_ERR_INVALID_PRIO,
    RTOS_ERR_INVALID_STATE
} RTOS_Status_t;

#endif