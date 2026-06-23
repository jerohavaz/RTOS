#ifndef OS_TYPES_H_
#define OS_TYPES_H_

typedef enum {
    OS_OK = 0,

    /* Generic argument / object errors */
    OS_ERR_NULL,          /* required pointer is NULL */
    OS_ERR_INVALID_ARG,   /* bad size, count, timeout, priority, etc. */
    OS_ERR_INVALID_STATE, /* object/task/kernel not in valid state */

    /* Blocking / waiting */
    OS_ERR_TIMEOUT,     /* timed wait expired */
    OS_ERR_WOULD_BLOCK, /* non-blocking operation cannot proceed now */

    /* Capacity / availability */
    OS_ERR_FULL,  /* queue full, semaphore max reached */
    OS_ERR_EMPTY, /* queue empty, optional if using WOULD_BLOCK */

    /* Ownership / permissions */
    OS_ERR_NOT_OWNER,  /* mutex unlock by non-owner */

    /* Scheduler / task */
    OS_ERR_INVALID_PRIO, /* invalid priority */
    OS_ERR_BUSY,         /* object/task already in use */
    OS_ERR_IN_ISR        /* operation not allowed from interrupt context */
} os_status_t;

#define OS_NO_WAIT      0u
#define OS_WAIT_FOREVER UINT32_MAX

#endif