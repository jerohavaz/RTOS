/**
 * @file integration_scheduler.c
 * @brief Scheduler priority and one-tick round-robin integration test.
 * @author Jerome
 *
 * @details
 * Four tasks exercise fixed-priority dispatch and round-robin scheduling. The
 * controller must start first, two CPU-bound tasks of equal priority must both
 * receive CPU time, and the low-priority task may run only after both peers
 * have blocked. A multi-round handshake keeps both CPU-bound peers READY and
 * forces several tick-driven handoffs before either peer may block.
 *
 * @par Test sequence
 * 1. The controller verifies that no lower-priority task ran first.
 * 2. The controller blocks for several ticks.
 * 3. Equal-priority CPU-bound peers complete several round-robin handshakes.
 * 4. Both peers park, allowing the low-priority task to run.
 * 5. The controller wakes and validates all observations.
 */

#include "integration_test.h"
#include "integration_tests.h"
#include "project.h"

#if PROJECT == PROJECT_SCHEDULER

#include "os_delay.h"
#include "os_task.h"
#include "os_types.h"

#include <stdint.h>

#define SCHED_CONTROL_PRIORITY (4u)       /**< Coordinator task priority. */
#define SCHED_RR_PRIORITY      (3u)       /**< Shared priority of both round-robin peers. */
#define SCHED_LOW_PRIORITY     (2u)       /**< Priority of the starvation/order probe. */
#define SCHED_OBSERVE_TICKS    (8u)       /**< Controller blocking interval for observation. */
#define SCHED_PARK_TICKS       (1000000u) /**< Long blocking interval for completed tasks. */
#define SCHED_RR_ROUNDS        (4u)       /**< Required READY-to-READY handshakes. */

/** @brief Debugger-visible scheduler observations. */
typedef struct {
    volatile uint32_t controller_started;       /**< Nonzero after the controller starts. */
    volatile uint32_t rr_runs[2];               /**< Execution counters of the two peers. */
    volatile uint32_t rr_round[2];              /**< Completed handshake phase per peer. */
    volatile uint32_t rr_blocked[2];            /**< Nonzero immediately before peer parking. */
    volatile uint32_t low_runs;                 /**< Number of low-priority task iterations. */
    volatile uint32_t priority_order_violation; /**< Nonzero if the low task ran too early. */
} scheduler_test_observation_t;

/**
 * @brief Live scheduler observations for debugger inspection.
 * @note The aggregate verdict is stored in @ref g_integration_test_result.
 */
scheduler_test_observation_t g_scheduler_test_observation;

/**
 * @brief Keep a completed test task out of the ready set.
 *
 * Repeated long scheduler delays avoid wasting CPU time while retaining the
 * task and its stack for debugger inspection.
 *
 * @note This function does not return.
 */
static void scheduler_park(void) {
    while (1) {
        integration_test_check(os_delay(SCHED_PARK_TICKS) == OS_OK);
    }
}

/**
 * @brief First equal-priority CPU-bound task.
 *
 * Every phase waits for task B to publish the same phase. Neither task yields
 * or blocks during the handshake, so each phase requires another scheduler
 * dispatch while both tasks remain READY.
 */
static void scheduler_rr_task_a(void) {
    uint32_t round;

    for (round = 1u; round <= SCHED_RR_ROUNDS; round++) {
        g_scheduler_test_observation.rr_round[0] = round;

        while (g_scheduler_test_observation.rr_round[1] < round) {
            /* CPU-bound by design: only a tick may rotate to the READY peer. */
            g_scheduler_test_observation.rr_runs[0]++;
        }

        g_scheduler_test_observation.rr_runs[0]++;
    }

    g_scheduler_test_observation.rr_blocked[0] = 1u;
    scheduler_park();
}

/**
 * @brief Second equal-priority CPU-bound round-robin task.
 *
 * This task mirrors scheduler_rr_task_a() and cannot finish until all required
 * READY-to-READY handshakes have occurred.
 */
static void scheduler_rr_task_b(void) {
    uint32_t round;

    for (round = 1u; round <= SCHED_RR_ROUNDS; round++) {
        g_scheduler_test_observation.rr_round[1] = round;

        while (g_scheduler_test_observation.rr_round[0] < round) {
            /* CPU-bound by design: only a tick may rotate to the READY peer. */
            g_scheduler_test_observation.rr_runs[1]++;
        }

        g_scheduler_test_observation.rr_runs[1]++;
    }

    g_scheduler_test_observation.rr_blocked[1] = 1u;
    scheduler_park();
}

/**
 * @brief Detect execution while a higher-priority peer is still runnable.
 *
 * The task records a sticky violation if either round-robin peer has not yet
 * parked. It then blocks for one tick so the controller can resume promptly.
 *
 * @note This function does not return.
 */
static void scheduler_low_task(void) {
    while (1) {
        if ((g_scheduler_test_observation.rr_blocked[0] == 0u) ||
            (g_scheduler_test_observation.rr_blocked[1] == 0u)) {
            g_scheduler_test_observation.priority_order_violation = 1u;
        }

        g_scheduler_test_observation.low_runs++;
        integration_test_check(os_delay(1u) == OS_OK);
    }
}

/**
 * @brief Coordinate and evaluate the scheduler scenario.
 *
 * @post The aggregate result is passed if priority ordering and round-robin
 *       progress were both observed; otherwise at least one check has failed.
 * @note This function parks permanently after evaluation.
 */
static void scheduler_control_task(void) {
    g_scheduler_test_observation.controller_started = 1u;

    /* The highest-priority task must execute before every lower-priority task. */
    integration_test_check(g_scheduler_test_observation.rr_runs[0] == 0u);
    integration_test_check(g_scheduler_test_observation.rr_runs[1] == 0u);
    integration_test_check(g_scheduler_test_observation.low_runs == 0u);

    integration_test_check(os_delay(SCHED_OBSERVE_TICKS) == OS_OK);

    /* Both CPU-bound peers must have received a tick-sized time slice. */
    integration_test_check(g_scheduler_test_observation.rr_runs[0] > 0u);
    integration_test_check(g_scheduler_test_observation.rr_runs[1] > 0u);
    integration_test_check(g_scheduler_test_observation.rr_round[0] == SCHED_RR_ROUNDS);
    integration_test_check(g_scheduler_test_observation.rr_round[1] == SCHED_RR_ROUNDS);
    integration_test_check(g_scheduler_test_observation.rr_blocked[0] != 0u);
    integration_test_check(g_scheduler_test_observation.rr_blocked[1] != 0u);

    /* Low priority may run only after both round-robin tasks have blocked. */
    integration_test_check(g_scheduler_test_observation.priority_order_violation == 0u);
    integration_test_check(g_scheduler_test_observation.low_runs > 0u);

    integration_test_pass();
    scheduler_park();
}

/** @copydoc integration_scheduler_init */
void integration_scheduler_init(void) {
    g_scheduler_test_observation.controller_started = 0u;
    g_scheduler_test_observation.rr_runs[0] = 0u;
    g_scheduler_test_observation.rr_runs[1] = 0u;
    g_scheduler_test_observation.rr_round[0] = 0u;
    g_scheduler_test_observation.rr_round[1] = 0u;
    g_scheduler_test_observation.rr_blocked[0] = 0u;
    g_scheduler_test_observation.rr_blocked[1] = 0u;
    g_scheduler_test_observation.low_runs = 0u;
    g_scheduler_test_observation.priority_order_violation = 0u;

    integration_test_check(os_task_create(scheduler_control_task, SCHED_CONTROL_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(scheduler_rr_task_a, SCHED_RR_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(scheduler_rr_task_b, SCHED_RR_PRIORITY) == OS_OK);
    integration_test_check(os_task_create(scheduler_low_task, SCHED_LOW_PRIORITY) == OS_OK);
}

#endif /* PROJECT == PROJECT_SCHEDULER */
