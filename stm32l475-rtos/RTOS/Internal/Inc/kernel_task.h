/**
 * @file kernel_task.h
 * @brief Internal kernel task representation and blocking-wait context.
 * @author Jerome
 *
 * @details
 * Defines the kernel-owned task object that extends the architecture-neutral
 * task control block with intrusive list nodes, timeout state, and information
 * describing a pending blocking operation.
 *
 * A task can be linked through @ref kernel_task::sched_node in exactly one
 * scheduler-domain list at a time: the global ready queue or one synchronization
 * object's wait queue. The independent @ref kernel_task::timeout_node allows the
 * same blocked task to be present in the global timeout list simultaneously.
 *
 * @warning These definitions are internal kernel state. Application code must
 *          not read or modify their fields directly.
 */

#ifndef KERNEL_TASK_H_
#define KERNEL_TASK_H_

#include "os_types.h"
#include "tcb.h"

#include <stdint.h>

/**
 * @brief Operation for which a task is currently blocked.
 *
 * The timeout subsystem uses this value to dispatch object-specific cleanup
 * when a finite wait expires.
 */
typedef enum {
    K_WAIT_NONE = 0,   /**< Task has no active blocking operation. */
    K_WAIT_DELAY,      /**< Task is waiting for an @c os_delay() timeout. */
    K_WAIT_SEM,        /**< Task is waiting to acquire a semaphore. */
    K_WAIT_MUTEX,      /**< Task is waiting to acquire a mutex. */
    K_WAIT_QUEUE_SEND, /**< Task is waiting to send to a full queue. */
    K_WAIT_QUEUE_RECV  /**< Task is waiting to receive from an empty queue. */
} kernel_wait_type_t;

/**
 * @brief Forward declaration of the internal kernel task object.
 */
typedef struct kernel_task kernel_task_t;

/**
 * @brief Intrusive doubly linked-list node embedded in a kernel task.
 *
 * The list implementation obtains an embedded node through a
 * @c task_node_fn_t callback. A node is unlinked when both pointers are null.
 * When linked, the pointers reference the neighboring tasks rather than
 * neighboring node objects.
 */
typedef struct {
    kernel_task_t *next; /**< Next task in the owning circular list. */
    kernel_task_t *prev; /**< Previous task in the owning circular list. */
} kernel_task_list_node_t;

/**
 * @struct kernel_task
 * @brief Complete internal state associated with one schedulable task.
 *
 * @details
 * The object is allocated from the static task array in @c k_task.c. Its
 * fields are protected by kernel critical sections whenever they can be
 * accessed concurrently by task and exception context.
 *
 * Blocking services populate @ref wait_type, @ref wait_object,
 * @ref wait_result, and, for queues, @ref wait_data before marking the task
 * blocked. Object release or timeout cleanup resets the wait metadata before
 * returning the task to the ready state.
 */
struct kernel_task {
    /**
     * @brief Core task control block.
     *
     * Stores task identity, priority, scheduling state, stack storage, and the
     * saved stack pointer used by the Cortex-M context switch.
     */
    tcb_t tcb;

    /**
     * @brief Node used by ready queues and synchronization-object wait queues.
     *
     * This node is shared between those mutually exclusive list domains. A
     * task must never be ready and blocked on an object at the same time.
     */
    kernel_task_list_node_t sched_node;

    /**
     * @brief Node used exclusively by the global ordered timeout list.
     *
     * A blocked task can be linked through this node while @ref sched_node is
     * linked in a semaphore, mutex, or queue wait list.
     */
    kernel_task_list_node_t timeout_node;

    /**
     * @brief Absolute kernel tick at which a finite wait expires.
     *
     * The timeout list orders this value using signed tick differences so
     * wrap-around is handled for waits shorter than @c K_TIMEOUT_MAX.
     * The field is reset to zero when removed; timeout-node linkage remains
     * the authoritative indication of list membership because a valid
     * wrap-around expiration tick can also equal zero.
     */
    uint32_t wake_tick;

    /**
     * @brief Kind of blocking operation currently in progress.
     *
     * Must be @ref K_WAIT_NONE whenever the task is not blocked on a delay or
     * synchronization object.
     */
    kernel_wait_type_t wait_type;

    /**
     * @brief Synchronization object associated with the active wait.
     *
     * Points to the relevant semaphore, mutex, or queue. Delay waits and tasks
     * with no active wait store null.
     */
    void *wait_object;

    /**
     * @brief Status returned when the blocked operation resumes.
     *
     * Blocking services initialize this to @c OS_ERR_BUSY. The wakeup path
     * replaces it with @c OS_OK or @c OS_ERR_TIMEOUT before readying the task.
     */
    os_status_t wait_result;

    /**
     * @brief Queue message buffer associated with a blocked queue operation.
     *
     * For @ref K_WAIT_QUEUE_SEND, points to the source message supplied by the
     * blocked sender. For @ref K_WAIT_QUEUE_RECV, points to the receiver's
     * destination buffer. All other wait types store null. The pointed-to
     * storage must remain valid until the task resumes or the wait times out.
     */
    void *wait_data;
};

#endif /* KERNEL_TASK_H_ */