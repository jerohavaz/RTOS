/**
 * @file integration_semaphore.c
 * @brief Minimal deterministic binary/counting semaphore integration test.
 * @author Jerome
 *
 * @details
 * A high-priority probe checks stored-count bounds and then parks. The high
 * waiter is held outside the tested wait queue by a start gate, allowing the
 * low waiter to enqueue first. A lower-priority controller opens the gate,
 * releases two tokens, and proves that priority wins over FIFO. The controller
 * finally performs a finite acquire on the empty semaphore to emit the timeout
 * path. No polling or assumed one-tick scheduling window is used.
 */

#include "integration_test.h"
#include "integration_tests.h"
#include "project.h"

#if PROJECT == PROJECT_SEMAPHORE

#include "os_delay.h"
#include "os_sem.h"
#include "os_task.h"
#include "os_types.h"

#include <stdint.h>

#define SEM_PROBE_PRIORITY     (8u)
#define SEM_HIGH_PRIORITY      (7u)
#define SEM_LOW_PRIORITY       (6u)
#define SEM_CONTROL_PRIORITY   (4u)
#define SEM_BINARY_MAX_COUNT   (1u)
#define SEM_COUNTING_MAX_COUNT (3u)
#define SEM_TIMEOUT_TICKS      (2u)
#define SEM_PARK_TICKS         (1000000u)
#define SEM_WAITER_HIGH_MARKER (1u)
#define SEM_WAITER_LOW_MARKER  (2u)

/** @brief Observable waiter progress. */
typedef enum {
    SEM_WAITER_NOT_STARTED = 0u,
    SEM_WAITER_GATED,
    SEM_WAITER_WAITING,
    SEM_WAITER_ACQUIRED
} semaphore_waiter_state_t;

/** @brief Debugger-visible semaphore observations. */
typedef struct {
    volatile uint32_t probe_complete;
    volatile semaphore_waiter_state_t high_state;
    volatile semaphore_waiter_state_t low_state;
    volatile uint32_t acquisition_order[2];
    volatile uint32_t acquisition_count;
    volatile uint32_t timeout_complete;
} semaphore_test_observation_t;

semaphore_test_observation_t g_semaphore_test_observation;

static os_sem_t g_binary_semaphore;
static os_sem_t g_counting_semaphore;
static os_sem_t g_high_start_gate;

/** @brief Permanently block a completed test task. */
static void semaphore_park(void) {
    while (1) {
        integration_test_check(os_delay(SEM_PARK_TICKS) == OS_OK);
    }
}

/** @brief Record one successful waiter acquisition without overrunning storage. */
static void semaphore_record_acquisition(uint32_t marker) {
    uint32_t index = g_semaphore_test_observation.acquisition_count;

    integration_test_check(index < 2u);
    if (index < 2u) {
        g_semaphore_test_observation.acquisition_order[index] = marker;
    }
    g_semaphore_test_observation.acquisition_count = index + 1u;
}

/** @brief Check binary and counting stored-count behavior before contention. */
static void semaphore_probe_task(void) {
    integration_test_check(os_sem_acquire(&g_binary_semaphore, OS_NO_WAIT) == OS_ERR_WOULD_BLOCK);
    integration_test_check(os_sem_release(&g_binary_semaphore) == OS_OK);
    integration_test_check(os_sem_release(&g_binary_semaphore) == OS_ERR_FULL);
    integration_test_check(os_sem_acquire(&g_binary_semaphore, OS_NO_WAIT) == OS_OK);

    integration_test_check(os_sem_release(&g_counting_semaphore) == OS_OK);
    integration_test_check(os_sem_release(&g_counting_semaphore) == OS_OK);
    integration_test_check(os_sem_release(&g_counting_semaphore) == OS_OK);
    integration_test_check(os_sem_release(&g_counting_semaphore) == OS_ERR_FULL);
    integration_test_check(os_sem_acquire(&g_counting_semaphore, OS_NO_WAIT) == OS_OK);
    integration_test_check(os_sem_acquire(&g_counting_semaphore, OS_NO_WAIT) == OS_OK);
    integration_test_check(os_sem_acquire(&g_counting_semaphore, OS_NO_WAIT) == OS_OK);
    integration_test_check(os_sem_acquire(&g_counting_semaphore, OS_NO_WAIT) == OS_ERR_WOULD_BLOCK);

    g_semaphore_test_observation.probe_complete = 1u;
    semaphore_park();
}

/** @brief Join the tested queue second, then acquire by higher priority. */
static void semaphore_high_waiter_task(void) {
    g_semaphore_test_observation.high_state = SEM_WAITER_GATED;
    integration_test_check(os_sem_acquire(&g_high_start_gate, OS_WAIT_FOREVER) == OS_OK);

    g_semaphore_test_observation.high_state = SEM_WAITER_WAITING;
    integration_test_check(os_sem_acquire(&g_binary_semaphore, OS_WAIT_FOREVER) == OS_OK);

    semaphore_record_acquisition(SEM_WAITER_HIGH_MARKER);
    g_semaphore_test_observation.high_state = SEM_WAITER_ACQUIRED;
    semaphore_park();
}

/** @brief Join the tested queue first as the lower-priority waiter. */
static void semaphore_low_waiter_task(void) {
    g_semaphore_test_observation.low_state = SEM_WAITER_WAITING;
    integration_test_check(os_sem_acquire(&g_binary_semaphore, OS_WAIT_FOREVER) == OS_OK);

    semaphore_record_acquisition(SEM_WAITER_LOW_MARKER);
    g_semaphore_test_observation.low_state = SEM_WAITER_ACQUIRED;
    semaphore_park();
}

/** @brief Drive ordered wake-up and the finite-timeout path. */
static void semaphore_control_task(void) {
    /* Every higher-priority task reached a blocking point before this runs. */
    integration_test_check(g_semaphore_test_observation.probe_complete != 0u);
    integration_test_check(g_semaphore_test_observation.high_state == SEM_WAITER_GATED);
    integration_test_check(g_semaphore_test_observation.low_state == SEM_WAITER_WAITING);

    /* High joins after low; priority must nevertheless wake high first. */
    integration_test_check(os_sem_release(&g_high_start_gate) == OS_OK);
    integration_test_check(g_semaphore_test_observation.high_state == SEM_WAITER_WAITING);

    integration_test_check(os_sem_release(&g_binary_semaphore) == OS_OK);
    integration_test_check(g_semaphore_test_observation.high_state == SEM_WAITER_ACQUIRED);
    integration_test_check(g_semaphore_test_observation.low_state == SEM_WAITER_WAITING);

    integration_test_check(os_sem_release(&g_binary_semaphore) == OS_OK);
    integration_test_check(g_semaphore_test_observation.low_state == SEM_WAITER_ACQUIRED);
    integration_test_check(g_semaphore_test_observation.acquisition_count == 2u);
    integration_test_check(g_semaphore_test_observation.acquisition_order[0] ==
                           SEM_WAITER_HIGH_MARKER);
    integration_test_check(g_semaphore_test_observation.acquisition_order[1] ==
                           SEM_WAITER_LOW_MARKER);

    integration_test_check(os_sem_acquire(&g_binary_semaphore, SEM_TIMEOUT_TICKS) ==
                           OS_ERR_TIMEOUT);
    g_semaphore_test_observation.timeout_complete = 1u;

    integration_test_pass();
    semaphore_park();
}

void integration_semaphore_init(void) {
    g_semaphore_test_observation.probe_complete = 0u;
    g_semaphore_test_observation.high_state = SEM_WAITER_NOT_STARTED;
    g_semaphore_test_observation.low_state = SEM_WAITER_NOT_STARTED;
    g_semaphore_test_observation.acquisition_order[0] = 0u;
    g_semaphore_test_observation.acquisition_order[1] = 0u;
    g_semaphore_test_observation.acquisition_count = 0u;
    g_semaphore_test_observation.timeout_complete = 0u;

    integration_test_check(os_sem_init(&g_binary_semaphore, 0u, SEM_BINARY_MAX_COUNT) == OS_OK);
    integration_test_check(os_sem_init(&g_counting_semaphore, 0u, SEM_COUNTING_MAX_COUNT) == OS_OK);
    integration_test_check(os_sem_init(&g_high_start_gate, 0u, SEM_BINARY_MAX_COUNT) == OS_OK);

    integration_test_check(os_task_create(semaphore_probe_task, SEM_PROBE_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(semaphore_high_waiter_task, SEM_HIGH_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(semaphore_low_waiter_task, SEM_LOW_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(semaphore_control_task, SEM_CONTROL_PRIORITY) == OS_OK);
}

#endif /* PROJECT == PROJECT_SEMAPHORE */
