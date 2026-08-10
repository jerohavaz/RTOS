/**
 * @file integration_delay.c
 * @brief Blocking and busy-delay integration test.
 * @author Jerome
 *
 * @details
 * A high-priority controller alternates between a busy delay and a
 * scheduler-aware delay while a lower-priority observer counts executions.
 * The observer must remain unchanged during the busy delay and must progress
 * while the controller is blocked by @c os_delay().
 *
 * @par Test sequence
 * 1. Record the observer counter and busy-wait for a fixed duration.
 * 2. Verify that the lower-priority observer did not execute.
 * 3. Block the controller for the same duration.
 * 4. Verify that the observer executed before the controller became ready.
 *
 * @note Existing task-state and tick trace events prove the exact temporal
 *       properties: busy delay causes no task-state transition, normal delay
 *       remains BLOCKED for the requested ticks, and becomes READY afterward.
 */

#include "integration_test.h"
#include "integration_tests.h"
#include "project.h"

#if PROJECT == PROJECT_DELAY

#include "os_delay.h"
#include "os_task.h"
#include "os_types.h"

#include <stdint.h>

#define DELAY_CONTROL_PRIORITY  (4u) /**< Priority of the delay-driving task. */
#define DELAY_OBSERVER_PRIORITY (2u) /**< Priority of the progress observer. */
#define DELAY_TEST_TICKS        (5u) /**< Duration used by both delay variants. */

/** @brief Current phase of the delay integration test. */
typedef enum {
    DELAY_PHASE_NOT_STARTED = 0u, /**< Tasks have not started the first cycle. */
    DELAY_PHASE_BUSY,             /**< Controller is executing the busy delay. */
    DELAY_PHASE_BLOCKING,         /**< Controller is blocked in @c os_delay(). */
    DELAY_PHASE_COMPLETE          /**< At least one complete cycle passed. */
} delay_test_phase_t;

/** @brief Debugger-visible delay-test observations. */
typedef struct {
    volatile delay_test_phase_t phase;  /**< Current controller phase. */
    volatile uint32_t observer_runs;    /**< Observer execution counter. */
    volatile uint32_t completed_cycles; /**< Successfully evaluated cycles. */
} delay_test_observation_t;

/** @brief Live delay observations for debugger inspection. */
delay_test_observation_t g_delay_test_observation;

/**
 * @brief Count opportunities created by a blocked controller.
 *
 * The observer delays for one tick after every increment so it remains
 * cooperative and repeatedly exposes blocking-delay scheduling behavior.
 *
 * @note This function does not return.
 */
static void delay_observer_task(void) {
    while (1) {
        g_delay_test_observation.observer_runs++;
        integration_test_check(os_delay(1u) == OS_OK);
    }
}

/**
 * @brief Exercise and compare busy and scheduler-based delays repeatedly.
 *
 * @post Every completed cycle increments @c completed_cycles.
 * @post The first successful cycle marks the aggregate test as passed.
 * @note A later failed repetition changes the sticky aggregate verdict to
 *       @ref INTEGRATION_TEST_FAILED.
 */
static void delay_control_task(void) {
    while (1) {
        uint32_t before = g_delay_test_observation.observer_runs;

        g_delay_test_observation.phase = DELAY_PHASE_BUSY;
        integration_test_check(os_delay_busy(DELAY_TEST_TICKS) == OS_OK);

        /* A lower-priority task must not run while this task busy-waits. */
        integration_test_check(g_delay_test_observation.observer_runs == before);

        before = g_delay_test_observation.observer_runs;
        g_delay_test_observation.phase = DELAY_PHASE_BLOCKING;
        integration_test_check(os_delay(DELAY_TEST_TICKS) == OS_OK);

        /* A scheduler delay must block this task and let the observer run. */
        integration_test_check(g_delay_test_observation.observer_runs > before);

        g_delay_test_observation.completed_cycles++;
        g_delay_test_observation.phase = DELAY_PHASE_COMPLETE;
        integration_test_pass();
    }
}

/** @copydoc integration_delay_init */
void integration_delay_init(void) {
    g_delay_test_observation.phase = DELAY_PHASE_NOT_STARTED;
    g_delay_test_observation.observer_runs = 0u;
    g_delay_test_observation.completed_cycles = 0u;

    integration_test_check(os_task_create(delay_control_task, DELAY_CONTROL_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(delay_observer_task, DELAY_OBSERVER_PRIORITY) == OS_OK);
}

#endif /* PROJECT == PROJECT_DELAY */
