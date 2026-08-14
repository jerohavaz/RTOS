/**
 * @file integration_mutex.c
 * @brief Minimal deterministic mutex contention and ownership test.
 * @author Jerome
 *
 * @details
 * The owner locks first and blocks on a release gate. The high waiter is held
 * on a second gate so the low waiter enters the mutex wait queue first. An
 * intruder checks owner-only unlock and a finite lock timeout while the mutex
 * remains owned. The lowest-priority controller then admits the high waiter
 * and releases the owner. Direct handoff must select high before low despite
 * FIFO insertion order.
 */

#include "integration_test.h"
#include "integration_tests.h"
#include "project.h"

#if PROJECT == PROJECT_MUTEX

#include "os_delay.h"
#include "os_mutex.h"
#include "os_sem.h"
#include "os_task.h"
#include "os_types.h"

#include <stdint.h>

#define MUTEX_OWNER_PRIORITY     (8u)
#define MUTEX_HIGH_PRIORITY      (7u)
#define MUTEX_LOW_PRIORITY       (6u)
#define MUTEX_INTRUDER_PRIORITY  (5u)
#define MUTEX_CONTROL_PRIORITY   (3u)
#define MUTEX_TIMEOUT_TICKS      (2u)
#define MUTEX_TIMEOUT_WAIT_TICKS (3u)
#define MUTEX_PARK_TICKS         (1000000u)
#define MUTEX_WAITER_HIGH_MARKER (1u)
#define MUTEX_WAITER_LOW_MARKER  (2u)

/** @brief Observable participant progress. */
typedef enum {
    MUTEX_TASK_NOT_STARTED = 0u,
    MUTEX_TASK_GATED,
    MUTEX_TASK_WAITING,
    MUTEX_TASK_COMPLETE
} mutex_task_state_t;

/** @brief Debugger-visible mutex observations. */
typedef struct {
    volatile mutex_task_state_t owner_state;
    volatile mutex_task_state_t high_state;
    volatile mutex_task_state_t low_state;
    volatile mutex_task_state_t intruder_state;
    volatile uint32_t acquisition_order[2];
    volatile uint32_t acquisition_count;
    volatile uint32_t active_owners;
} mutex_test_observation_t;

mutex_test_observation_t g_mutex_test_observation;

static os_mutex_t g_test_mutex;
static os_sem_t g_owner_release_gate;
static os_sem_t g_high_start_gate;

/** @brief Permanently block a completed participant. */
static void mutex_park(void) {
    while (1) {
        integration_test_check(os_delay(MUTEX_PARK_TICKS) == OS_OK);
    }
}

/** @brief Enter the observed critical section and enforce single ownership. */
static void mutex_enter_critical(void) {
    g_mutex_test_observation.active_owners++;
    integration_test_check(g_mutex_test_observation.active_owners == 1u);
}

/** @brief Leave the observed critical section. */
static void mutex_leave_critical(void) {
    integration_test_check(g_mutex_test_observation.active_owners == 1u);
    g_mutex_test_observation.active_owners--;
}

/** @brief Record one waiter handoff without overrunning storage. */
static void mutex_record_acquisition(uint32_t marker) {
    uint32_t index = g_mutex_test_observation.acquisition_count;

    integration_test_check(index < 2u);
    if (index < 2u) {
        g_mutex_test_observation.acquisition_order[index] = marker;
    }
    g_mutex_test_observation.acquisition_count = index + 1u;
}

/** @brief Acquire first, retain ownership while blocked, then release. */
static void mutex_owner_task(void) {
    integration_test_check(os_mutex_lock(&g_test_mutex, OS_NO_WAIT) == OS_OK);
    mutex_enter_critical();
    g_mutex_test_observation.owner_state = MUTEX_TASK_GATED;

    integration_test_check(os_sem_acquire(&g_owner_release_gate, OS_WAIT_FOREVER) == OS_OK);

    integration_test_check(os_mutex_lock(&g_test_mutex, OS_NO_WAIT) == OS_ERR_INVALID_STATE);
    mutex_leave_critical();
    integration_test_check(os_mutex_unlock(&g_test_mutex) == OS_OK);

    g_mutex_test_observation.owner_state = MUTEX_TASK_COMPLETE;
    mutex_park();
}

/** @brief Join second but receive the first handoff by priority. */
static void mutex_high_waiter_task(void) {
    g_mutex_test_observation.high_state = MUTEX_TASK_GATED;
    integration_test_check(os_sem_acquire(&g_high_start_gate, OS_WAIT_FOREVER) == OS_OK);

    g_mutex_test_observation.high_state = MUTEX_TASK_WAITING;
    integration_test_check(os_mutex_lock(&g_test_mutex, OS_WAIT_FOREVER) == OS_OK);
    mutex_enter_critical();
    mutex_record_acquisition(MUTEX_WAITER_HIGH_MARKER);
    mutex_leave_critical();
    integration_test_check(os_mutex_unlock(&g_test_mutex) == OS_OK);

    g_mutex_test_observation.high_state = MUTEX_TASK_COMPLETE;
    mutex_park();
}

/** @brief Join the mutex wait queue first as the lower-priority waiter. */
static void mutex_low_waiter_task(void) {
    g_mutex_test_observation.low_state = MUTEX_TASK_WAITING;
    integration_test_check(os_mutex_lock(&g_test_mutex, OS_WAIT_FOREVER) == OS_OK);
    mutex_enter_critical();
    mutex_record_acquisition(MUTEX_WAITER_LOW_MARKER);
    mutex_leave_critical();
    integration_test_check(os_mutex_unlock(&g_test_mutex) == OS_OK);

    g_mutex_test_observation.low_state = MUTEX_TASK_COMPLETE;
    mutex_park();
}

/** @brief Check non-owner unlock and timeout while the owner retains the mutex. */
static void mutex_intruder_task(void) {
    integration_test_check(os_mutex_unlock(&g_test_mutex) == OS_ERR_NOT_OWNER);
    g_mutex_test_observation.intruder_state = MUTEX_TASK_WAITING;
    integration_test_check(os_mutex_lock(&g_test_mutex, MUTEX_TIMEOUT_TICKS) == OS_ERR_TIMEOUT);
    g_mutex_test_observation.intruder_state = MUTEX_TASK_COMPLETE;
    mutex_park();
}

/** @brief Admit high second, release the owner, and validate the handoffs. */
static void mutex_control_task(void) {
    /* All higher-priority participants are blocked when control first runs. */
    integration_test_check(g_mutex_test_observation.owner_state == MUTEX_TASK_GATED);
    integration_test_check(g_mutex_test_observation.high_state == MUTEX_TASK_GATED);
    integration_test_check(g_mutex_test_observation.low_state == MUTEX_TASK_WAITING);
    integration_test_check(g_mutex_test_observation.intruder_state == MUTEX_TASK_WAITING);

    /* Allow the finite intruder lock to expire while ownership is retained. */
    integration_test_check(os_delay(MUTEX_TIMEOUT_WAIT_TICKS) == OS_OK);
    integration_test_check(g_mutex_test_observation.intruder_state == MUTEX_TASK_COMPLETE);

    /* High joins after low, then owner unlock initiates priority handoff. */
    integration_test_check(os_sem_release(&g_high_start_gate) == OS_OK);
    integration_test_check(g_mutex_test_observation.high_state == MUTEX_TASK_WAITING);
    integration_test_check(os_sem_release(&g_owner_release_gate) == OS_OK);

    /* Owner, high, and low all outrank control and must have completed. */
    integration_test_check(g_mutex_test_observation.owner_state == MUTEX_TASK_COMPLETE);
    integration_test_check(g_mutex_test_observation.high_state == MUTEX_TASK_COMPLETE);
    integration_test_check(g_mutex_test_observation.low_state == MUTEX_TASK_COMPLETE);
    integration_test_check(g_mutex_test_observation.active_owners == 0u);
    integration_test_check(g_mutex_test_observation.acquisition_count == 2u);
    integration_test_check(g_mutex_test_observation.acquisition_order[0] ==
                           MUTEX_WAITER_HIGH_MARKER);
    integration_test_check(g_mutex_test_observation.acquisition_order[1] ==
                           MUTEX_WAITER_LOW_MARKER);

    /* The mutex is reusable after the final handoff. */
    integration_test_check(os_mutex_lock(&g_test_mutex, OS_NO_WAIT) == OS_OK);
    mutex_enter_critical();
    mutex_leave_critical();
    integration_test_check(os_mutex_unlock(&g_test_mutex) == OS_OK);

    integration_test_pass();
    mutex_park();
}

void integration_mutex_init(void) {
    g_mutex_test_observation.owner_state = MUTEX_TASK_NOT_STARTED;
    g_mutex_test_observation.high_state = MUTEX_TASK_NOT_STARTED;
    g_mutex_test_observation.low_state = MUTEX_TASK_NOT_STARTED;
    g_mutex_test_observation.intruder_state = MUTEX_TASK_NOT_STARTED;
    g_mutex_test_observation.acquisition_order[0] = 0u;
    g_mutex_test_observation.acquisition_order[1] = 0u;
    g_mutex_test_observation.acquisition_count = 0u;
    g_mutex_test_observation.active_owners = 0u;

    integration_test_check(os_mutex_init(&g_test_mutex) == OS_OK);
    integration_test_check(os_sem_init(&g_owner_release_gate, 0u, 1u) == OS_OK);
    integration_test_check(os_sem_init(&g_high_start_gate, 0u, 1u) == OS_OK);

    integration_test_check(os_task_create(mutex_owner_task, MUTEX_OWNER_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(mutex_high_waiter_task, MUTEX_HIGH_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(mutex_low_waiter_task, MUTEX_LOW_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(mutex_intruder_task, MUTEX_INTRUDER_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(mutex_control_task, MUTEX_CONTROL_PRIORITY) == OS_OK);
}

#endif /* PROJECT == PROJECT_MUTEX */
