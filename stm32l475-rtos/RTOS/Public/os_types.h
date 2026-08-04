/**
 * @file os_types.h
 * @brief Common RTOS status codes and timeout constants.
 * @author Jerome
 */
#ifndef OS_TYPES_H_
#define OS_TYPES_H_

#include <stdint.h>

/**
 * @brief Status values returned by RTOS services.
 */
typedef enum {
    OS_OK = 0, ///< Operation completed successfully.

    /* Generic argument / object errors */
    OS_ERR_NULL,          ///< A required pointer is null.
    OS_ERR_INVALID_ARG,   ///< A size, count, timeout, or other argument is invalid.
    OS_ERR_INVALID_STATE, ///< An object, task, or kernel state forbids the operation.

    /* Blocking / waiting */
    OS_ERR_TIMEOUT,     ///< A finite wait expired before completion.
    OS_ERR_WOULD_BLOCK, ///< A non-blocking operation cannot complete immediately.

    /* Capacity / availability */
    OS_ERR_FULL,  ///< An object has reached its configured capacity.
    OS_ERR_EMPTY, ///< An object contains no available item.

    /* Ownership / permissions */
    OS_ERR_NOT_OWNER, ///< The calling task does not own the requested object.

    /* Scheduler / task */
    OS_ERR_INVALID_PRIO, ///< A task priority is outside the configured range.
    OS_ERR_BUSY,         ///< An operation or internal wait is still in progress.
    OS_ERR_IN_ISR        ///< The operation is not permitted in exception context.
} os_status_t;

/** @brief Request that an operation return instead of blocking. */
#define OS_NO_WAIT 0u

/** @brief Request an indefinite wait from APIs that support blocking. */
#define OS_WAIT_FOREVER UINT32_MAX

#endif /* OS_TYPES_H_ */