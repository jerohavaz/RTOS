/**
 * @file integration_mutex.c
 * @brief Mutex ownership, contention, and blocking integration test.
 * @author Jerome
 *
 * @details
 * An owner holds a non-recursive mutex while two ordered waiters contend and
 * an unrelated task attempts an invalid unlock. The scenario verifies mutual
 * exclusion through an active-owner counter, owner-only release, recursive-
 * lock rejection, blocking access, priority-ordered handoff, and reuse after
 * all handoffs complete.
 *
 * @par Test sequence
 * 1. The highest-priority owner locks the mutex and blocks while holding it.
 * 2. Two waiters block in descending priority order.
 * 3. The lowest-priority intruder verifies owner-only unlock enforcement.
 * 4. The owner verifies non-recursive behavior and releases the mutex.
 * 5. Waiters acquire and release in priority order.
 * 6. The owner verifies that the mutex is available again.
 */

#include "integration_test.h"
#include "integration_tests.h"
#include "project.h"

#if PROJECT == PROJECT_MUTEX

#include "os_delay.h"
#include "os_mutex.h"
#include "os_task.h"
#include "os_types.h"

#include <stdint.h>

#define MUTEX_OWNER_PRIORITY     (6u)       /**< Initial owner/coordinator priority. */
#define MUTEX_HIGH_PRIORITY      (5u)       /**< Higher-priority waiter. */
#define MUTEX_LOW_PRIORITY       (4u)       /**< Lower-priority waiter. */
#define MUTEX_INTRUDER_PRIORITY  (3u)       /**< Non-owner unlock probe priority. */
#define MUTEX_HOLD_TICKS         (5u)       /**< Time the owner blocks while holding. */
#define MUTEX_PARK_TICKS         (1000000u) /**< Long delay after task completion. */
#define MUTEX_WAITER_HIGH_MARKER (1u)       /**< Acquisition marker of high waiter. */
#define MUTEX_WAITER_LOW_MARKER  (2u)       /**< Acquisition marker of low waiter. */

/** @brief Progress state of a mutex participant. */
typedef enum {
    MUTEX_TASK_NOT_STARTED = 0u, /**< Participant has not executed. */
    MUTEX_TASK_WAITING,          /**< Waiter is entering a blocking lock. */
    MUTEX_TASK_COMPLETE          /**< Participant completed its check. */
} mutex_task_state_t;

/** @brief Debugger-visible mutex-test observations. */
typedef struct {
    volatile mutex_task_state_t high_state;     /**< High-priority waiter progress. */
    volatile mutex_task_state_t low_state;      /**< Low-priority waiter progress. */
    volatile mutex_task_state_t intruder_state; /**< Non-owner probe progress. */
    volatile uint32_t acquisition_order[2];     /**< Waiter markers in ownership order. */
    volatile uint32_t acquisition_count;        /**< Recorded waiter acquisitions. */
    volatile uint32_t active_owners;            /**< Tasks currently inside the critical section. */
} mutex_test_observation_t;

/** @brief Live mutex observations for debugger inspection. */
mutex_test_observation_t g_mutex_test_observation;

/** @brief Non-recursive mutex shared by all scenario participants. */
static os_mutex_t g_test_mutex;

/**
 * @brief Keep a completed participant out of the ready set.
 * @note This function does not return.
 */
static void mutex_park(void) {
    while (1) {
        integration_test_check(os_delay(MUTEX_PARK_TICKS) == OS_OK);
    }
}

/**
 * @brief Append a waiter marker to the observed ownership order.
 * @param marker Unique marker of the waiter that acquired the mutex.
 * @post The acquisition counter is incremented exactly once.
 * @note An unexpected third acquisition fails safely without writing beyond
 *       @c acquisition_order.
 */
static void mutex_record_acquisition(uint32_t marker) {
    uint32_t index = g_mutex_test_observation.acquisition_count;

    integration_test_check(index < 2u);

    if (index < 2u) {
        g_mutex_test_observation.acquisition_order[index] = marker;
    }

    g_mutex_test_observation.acquisition_count = index + 1u;
}

/**
 * @brief Contend for the held mutex as the higher-priority waiter.
 * @post Successful handoff is recorded before this task releases the mutex.
 */
static void mutex_high_waiter_task(void) {
    g_mutex_test_observation.high_state = MUTEX_TASK_WAITING;
    integration_test_check(os_mutex_lock(&g_test_mutex, OS_WAIT_FOREVER) == OS_OK);

    g_mutex_test_observation.active_owners++;
    integration_test_check(g_mutex_test_observation.active_owners == 1u);
    mutex_record_acquisition(MUTEX_WAITER_HIGH_MARKER);
    g_mutex_test_observation.active_owners--;
    integration_test_check(os_mutex_unlock(&g_test_mutex) == OS_OK);
    g_mutex_test_observation.high_state = MUTEX_TASK_COMPLETE;
    mutex_park();
}

/**
 * @brief Contend for the held mutex as the lower-priority waiter.
 * @post Successful handoff is recorded before this task releases the mutex.
 */
static void mutex_low_waiter_task(void) {
    g_mutex_test_observation.low_state = MUTEX_TASK_WAITING;
    integration_test_check(os_mutex_lock(&g_test_mutex, OS_WAIT_FOREVER) == OS_OK);

    g_mutex_test_observation.active_owners++;
    integration_test_check(g_mutex_test_observation.active_owners == 1u);
    mutex_record_acquisition(MUTEX_WAITER_LOW_MARKER);
    g_mutex_test_observation.active_owners--;
    integration_test_check(os_mutex_unlock(&g_test_mutex) == OS_OK);
    g_mutex_test_observation.low_state = MUTEX_TASK_COMPLETE;
    mutex_park();
}

/**
 * @brief Verify that a task which does not own the mutex cannot unlock it.
 * @post @c intruder_state is complete after @ref OS_ERR_NOT_OWNER is observed.
 */
static void mutex_intruder_task(void) {
    integration_test_check(os_mutex_unlock(&g_test_mutex) == OS_ERR_NOT_OWNER);
    g_mutex_test_observation.intruder_state = MUTEX_TASK_COMPLETE;
    mutex_park();
}

/**
 * @brief Own, release, and finally re-acquire the mutex while coordinating.
 *
 * The owner deliberately blocks while retaining ownership so every lower
 * priority participant can exercise its path. Unlocking then causes direct,
 * priority-ordered ownership handoffs to the blocked waiters.
 *
 * @post The aggregate result is passed after all ownership and ordering checks.
 */
static void mutex_owner_task(void) {
    integration_test_check(os_mutex_lock(&g_test_mutex, OS_NO_WAIT) == OS_OK);
    g_mutex_test_observation.active_owners++;
    integration_test_check(g_mutex_test_observation.active_owners == 1u);

    /* A waiter returning here would increment active_owners to two and fail. */
    integration_test_check(os_delay(MUTEX_HOLD_TICKS) == OS_OK);

    integration_test_check(g_mutex_test_observation.high_state == MUTEX_TASK_WAITING);
    integration_test_check(g_mutex_test_observation.low_state == MUTEX_TASK_WAITING);
    integration_test_check(g_mutex_test_observation.intruder_state == MUTEX_TASK_COMPLETE);

    /* The mutex is deliberately non-recursive. */
    integration_test_check(os_mutex_lock(&g_test_mutex, OS_NO_WAIT) == OS_ERR_INVALID_STATE);
    integration_test_check(g_mutex_test_observation.active_owners == 1u);
    g_mutex_test_observation.active_owners--;
    integration_test_check(os_mutex_unlock(&g_test_mutex) == OS_OK);

    while (g_mutex_test_observation.acquisition_count < 2u) {
        integration_test_check(os_delay(1u) == OS_OK);
    }

    integration_test_check(g_mutex_test_observation.acquisition_order[0] ==
                           MUTEX_WAITER_HIGH_MARKER);
    integration_test_check(g_mutex_test_observation.acquisition_order[1] ==
                           MUTEX_WAITER_LOW_MARKER);
    integration_test_check(g_mutex_test_observation.high_state == MUTEX_TASK_COMPLETE);
    integration_test_check(g_mutex_test_observation.low_state == MUTEX_TASK_COMPLETE);
    integration_test_check(g_mutex_test_observation.active_owners == 0u);

    /* Both handoffs completed and the mutex must be available again. */
    integration_test_check(os_mutex_lock(&g_test_mutex, OS_NO_WAIT) == OS_OK);
    g_mutex_test_observation.active_owners++;
    integration_test_check(g_mutex_test_observation.active_owners == 1u);
    g_mutex_test_observation.active_owners--;
    integration_test_check(os_mutex_unlock(&g_test_mutex) == OS_OK);

    integration_test_pass();
    mutex_park();
}

/** @copydoc integration_mutex_init */
void integration_mutex_init(void) {
    g_mutex_test_observation.high_state = MUTEX_TASK_NOT_STARTED;
    g_mutex_test_observation.low_state = MUTEX_TASK_NOT_STARTED;
    g_mutex_test_observation.intruder_state = MUTEX_TASK_NOT_STARTED;
    g_mutex_test_observation.acquisition_order[0] = 0u;
    g_mutex_test_observation.acquisition_order[1] = 0u;
    g_mutex_test_observation.acquisition_count = 0u;
    g_mutex_test_observation.active_owners = 0u;

    integration_test_check(os_mutex_init(&g_test_mutex) == OS_OK);
    integration_test_check(os_task_create(mutex_owner_task, MUTEX_OWNER_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(mutex_high_waiter_task, MUTEX_HIGH_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(mutex_low_waiter_task, MUTEX_LOW_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(mutex_intruder_task, MUTEX_INTRUDER_PRIORITY) == OS_OK);
}

#endif /* PROJECT == PROJECT_MUTEX */
