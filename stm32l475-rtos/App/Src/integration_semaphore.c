/**
 * @file integration_semaphore.c
 * @brief Counting-semaphore contention and blocking integration test.
 * @author Jerome
 *
 * @details
 * The binary special case (maximum count one) is checked for empty, full,
 * consume, blocking, wake-up, and priority order. A second semaphore with a
 * maximum greater than one independently checks the counting range.
 *
 * @par Test sequence
 * 1. The controller checks the binary semaphore at counts zero and one.
 * 2. It delays so both lower-priority waiters can block on the empty object.
 * 3. Each controller release wakes the highest-priority remaining waiter.
 * 4. The separate counting semaphore is filled to its maximum and overflowed.
 * 5. Every stored token is consumed and a final no-wait acquire is rejected.
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

#define SEM_CONTROL_PRIORITY   (6u)       /**< Priority of the release coordinator. */
#define SEM_HIGH_PRIORITY      (5u)       /**< Priority of the first waiter. */
#define SEM_LOW_PRIORITY       (4u)       /**< Priority of the second waiter. */
#define SEM_BINARY_MAX_COUNT   (1u)       /**< Maximum count of the binary special case. */
#define SEM_COUNTING_MAX_COUNT (3u)       /**< Maximum count of the counting case. */
#define SEM_PARK_TICKS         (1000000u) /**< Long blocking interval after completion. */
#define SEM_WAITER_HIGH_MARKER (1u)       /**< Acquisition marker for the high waiter. */
#define SEM_WAITER_LOW_MARKER  (2u)       /**< Acquisition marker for the low waiter. */

/** @brief Per-waiter progress state. */
typedef enum {
    SEM_WAITER_NOT_STARTED = 0u, /**< Waiter task has not executed. */
    SEM_WAITER_BLOCKING,         /**< Waiter is entering its blocking acquire. */
    SEM_WAITER_ACQUIRED          /**< Waiter acquired a released token. */
} semaphore_waiter_state_t;

/** @brief Debugger-visible semaphore-test observations. */
typedef struct {
    volatile semaphore_waiter_state_t high_state; /**< High-priority waiter progress. */
    volatile semaphore_waiter_state_t low_state;  /**< Low-priority waiter progress. */
    volatile uint32_t acquisition_order[2];       /**< Markers in successful acquire order. */
    volatile uint32_t acquisition_count;          /**< Number of recorded acquisitions. */
} semaphore_test_observation_t;

/** @brief Live semaphore observations for debugger inspection. */
semaphore_test_observation_t g_semaphore_test_observation;

/** @brief Binary semaphore used for value bounds and ordered waiter wake-up. */
static os_sem_t g_binary_semaphore;

/** @brief Counting semaphore used only for the greater-than-one range test. */
static os_sem_t g_counting_semaphore;

/**
 * @brief Keep a completed participant blocked without consuming CPU time.
 * @note This function does not return.
 */
static void semaphore_park(void) {
    while (1) {
        integration_test_check(os_delay(SEM_PARK_TICKS) == OS_OK);
    }
}

/**
 * @brief Append a waiter marker to the observed acquisition order.
 * @param marker Unique marker of the waiter that acquired a token.
 * @post The acquisition counter is incremented exactly once.
 * @note Recording beyond the two expected waiters fails the test while
 *       avoiding an out-of-bounds array write.
 */
static void semaphore_record_acquisition(uint32_t marker) {
    uint32_t index = g_semaphore_test_observation.acquisition_count;

    integration_test_check(index < 2u);

    if (index < 2u) {
        g_semaphore_test_observation.acquisition_order[index] = marker;
    }

    g_semaphore_test_observation.acquisition_count = index + 1u;
}

/**
 * @brief Block the higher-priority waiter on the initially empty semaphore.
 * @post On release, the high marker is recorded and @c high_state becomes
 *       @ref SEM_WAITER_ACQUIRED.
 */
static void semaphore_high_waiter_task(void) {
    /* Let the low waiter enqueue first so wake order cannot pass by FIFO. */
    integration_test_check(os_delay(1u) == OS_OK);
    g_semaphore_test_observation.high_state = SEM_WAITER_BLOCKING;
    integration_test_check(os_sem_acquire(&g_binary_semaphore, OS_WAIT_FOREVER) == OS_OK);

    semaphore_record_acquisition(SEM_WAITER_HIGH_MARKER);
    g_semaphore_test_observation.high_state = SEM_WAITER_ACQUIRED;
    semaphore_park();
}

/**
 * @brief Block the lower-priority competing waiter on the semaphore.
 * @post On release, the low marker is recorded and @c low_state becomes
 *       @ref SEM_WAITER_ACQUIRED.
 */
static void semaphore_low_waiter_task(void) {
    g_semaphore_test_observation.low_state = SEM_WAITER_BLOCKING;
    integration_test_check(os_sem_acquire(&g_binary_semaphore, OS_WAIT_FOREVER) == OS_OK);

    semaphore_record_acquisition(SEM_WAITER_LOW_MARKER);
    g_semaphore_test_observation.low_state = SEM_WAITER_ACQUIRED;
    semaphore_park();
}

/**
 * @brief Release blocked waiters and validate counting-semaphore bounds.
 *
 * The controller first exercises the binary stored-count bounds, then blocks
 * itself so both waiters can reach their infinite acquires. After each release
 * it delays one tick, allowing the selected waiter to run and record wake-up.
 *
 * @post The aggregate result is passed only after wake order, maximum count,
 *       overflow rejection, token consumption, and empty rejection succeed.
 */
static void semaphore_control_task(void) {
    integration_test_check(os_sem_acquire(&g_binary_semaphore, OS_NO_WAIT) ==
                           OS_ERR_WOULD_BLOCK);
    integration_test_check(os_sem_release(&g_binary_semaphore) == OS_OK);
    integration_test_check(os_sem_release(&g_binary_semaphore) == OS_ERR_FULL);
    integration_test_check(os_sem_acquire(&g_binary_semaphore, OS_NO_WAIT) == OS_OK);

    /* Low enqueues first; a second delay lets the higher-priority waiter join. */
    integration_test_check(os_delay(1u) == OS_OK);
    integration_test_check(os_delay(1u) == OS_OK);
    integration_test_check(g_semaphore_test_observation.high_state == SEM_WAITER_BLOCKING);
    integration_test_check(g_semaphore_test_observation.low_state == SEM_WAITER_BLOCKING);

    integration_test_check(os_sem_release(&g_binary_semaphore) == OS_OK);
    integration_test_check(os_delay(1u) == OS_OK);
    integration_test_check(g_semaphore_test_observation.high_state == SEM_WAITER_ACQUIRED);
    integration_test_check(g_semaphore_test_observation.low_state == SEM_WAITER_BLOCKING);

    integration_test_check(os_sem_release(&g_binary_semaphore) == OS_OK);
    integration_test_check(os_delay(1u) == OS_OK);
    integration_test_check(g_semaphore_test_observation.low_state == SEM_WAITER_ACQUIRED);

    integration_test_check(g_semaphore_test_observation.acquisition_count == 2u);
    integration_test_check(g_semaphore_test_observation.acquisition_order[0] ==
                           SEM_WAITER_HIGH_MARKER);
    integration_test_check(g_semaphore_test_observation.acquisition_order[1] ==
                           SEM_WAITER_LOW_MARKER);

    /* Count 0 -> maximum, reject overflow, then consume all three tokens. */
    integration_test_check(os_sem_release(&g_counting_semaphore) == OS_OK);
    integration_test_check(os_sem_release(&g_counting_semaphore) == OS_OK);
    integration_test_check(os_sem_release(&g_counting_semaphore) == OS_OK);
    integration_test_check(os_sem_release(&g_counting_semaphore) == OS_ERR_FULL);
    integration_test_check(os_sem_acquire(&g_counting_semaphore, OS_NO_WAIT) == OS_OK);
    integration_test_check(os_sem_acquire(&g_counting_semaphore, OS_NO_WAIT) == OS_OK);
    integration_test_check(os_sem_acquire(&g_counting_semaphore, OS_NO_WAIT) == OS_OK);
    integration_test_check(os_sem_acquire(&g_counting_semaphore, OS_NO_WAIT) ==
                           OS_ERR_WOULD_BLOCK);

    integration_test_pass();
    semaphore_park();
}

/** @copydoc integration_semaphore_init */
void integration_semaphore_init(void) {
    g_semaphore_test_observation.high_state = SEM_WAITER_NOT_STARTED;
    g_semaphore_test_observation.low_state = SEM_WAITER_NOT_STARTED;
    g_semaphore_test_observation.acquisition_order[0] = 0u;
    g_semaphore_test_observation.acquisition_order[1] = 0u;
    g_semaphore_test_observation.acquisition_count = 0u;

    integration_test_check(os_sem_init(&g_binary_semaphore, 0u, SEM_BINARY_MAX_COUNT) == OS_OK);
    integration_test_check(os_sem_init(&g_counting_semaphore,
                                       0u,
                                       SEM_COUNTING_MAX_COUNT) == OS_OK);
    integration_test_check(os_task_create(semaphore_control_task, SEM_CONTROL_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(semaphore_high_waiter_task, SEM_HIGH_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(semaphore_low_waiter_task, SEM_LOW_PRIORITY) == OS_OK);
}

#endif /* PROJECT == PROJECT_SEMAPHORE */
