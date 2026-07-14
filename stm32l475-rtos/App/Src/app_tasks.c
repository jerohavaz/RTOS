#include "app_tasks.h"
#include "app_test_config.h"

#include "os_delay.h"
#include "os_task.h"

/*
 * If your delay API is declared somewhere else, include that header here.
 * Example:
 *   #include "os_delay.h"
 *   #include "os_time.h"
 */

static volatile uint32_t app_counter_1 = 0;
static volatile uint32_t app_counter_2 = 0;
static volatile uint32_t app_counter_3 = 0;

/*
 * Prevent the compiler from deleting empty busy loops.
 */
static void app_busy_step(volatile uint32_t *counter) {
    *counter = *counter + 1u;
}

/* ============================================================
 * Scenario 1:
 * Round-Robin + quantum
 *
 * Expected:
 *   task_rr_1 and task_rr_2 have same priority.
 *   They should alternate after quantum expiry.
 *
 * Verifies:
 *   - same-priority Round-Robin
 *   - quantum switching
 *   - no IDLE while tasks are READY
 * ============================================================ */

static void task_rr_1(void) {
    while (1) {
        app_busy_step(&app_counter_1);
    }
}

static void task_rr_2(void) {
    while (1) {
        app_busy_step(&app_counter_2);
    }
}

/* ============================================================
 * Scenario 2:
 * Priority scheduling
 *
 * Priority convention:
 *   lower number = lower priority
 *   higher number = higher priority
 *
 * Expected:
 *   task_high_prio should run before task_low_prio whenever both are READY.
 *
 * Verifies:
 *   - higher-priority READY task before lower-priority READY task
 *
 * Note:
 *   Since task_high_prio never blocks, task_low_prio should rarely or never run
 *   after task_high_prio becomes READY.
 * ============================================================ */

static void task_low_prio(void) {
    while (1) {
        app_busy_step(&app_counter_1);
    }
}

static void task_high_prio(void) {
    while (1) {
        app_busy_step(&app_counter_2);
    }
}

/* ============================================================
 * Scenario 3:
 * Idle after all tasks block on delay
 *
 * Expected:
 *   both tasks enter BLOCKED because of non-blocking delay.
 *   scheduler selects IDLE while no real task is READY.
 *   later, tasks become READY/RUNNING again.
 *
 * Verifies:
 *   - IDLE only when no real task READY
 *   - BLOCKED tasks do not RUN
 *   - valid BLOCKED -> READY transition after delay
 * ============================================================ */

static void task_delay_idle_1(void) {
    while (1) {
        app_busy_step(&app_counter_1);
        os_delay(APP_LONG_DELAY_TICKS);
    }
}

static void task_delay_idle_2(void) {
    while (1) {
        app_busy_step(&app_counter_2);
        os_delay(APP_LONG_DELAY_TICKS);
    }
}

/* ============================================================
 * Scenario 4:
 * Blocked task must not run
 *
 * Expected:
 *   task_blocked_subject repeatedly blocks on delay.
 *   task_background keeps running while subject is BLOCKED.
 *
 * Verifies:
 *   - BLOCKED task does not RUN
 *   - READY/BLOCKED/RUNNING state exclusivity
 * ============================================================ */

static void task_blocked_subject(void) {
    while (1) {
        app_busy_step(&app_counter_1);
        os_delay(APP_SHORT_DELAY_TICKS);
    }
}

static void task_background(void) {
    while (1) {
        app_busy_step(&app_counter_2);
    }
}

/* ============================================================
 * Scenario 5:
 * Mixed scheduler scenario
 *
 * Expected:
 *   high_1 and high_2 Round-Robin while both READY.
 *   low runs only when both high tasks are blocked.
 *
 * Verifies combined behavior:
 *   - priority
 *   - Round-Robin among same-priority high tasks
 *   - IDLE should not run while low is READY
 *   - blocked high tasks stop running during delay
 * ============================================================ */

static void task_mixed_high_1(void) {
    while (1) {
        app_busy_step(&app_counter_1);

        /*
         * Occasionally block so lower-priority task gets a chance.
         * Adjust this if trace gets too noisy.
         */
        if ((app_counter_1 % 10000u) == 0u) {
            os_delay(APP_SHORT_DELAY_TICKS);
        }
    }
}

static void task_mixed_high_2(void) {
    while (1) {
        app_busy_step(&app_counter_2);

        if ((app_counter_2 % 10000u) == 0u) {
            os_delay(APP_SHORT_DELAY_TICKS);
        }
    }
}

static void task_mixed_low(void) {
    while (1) {
        app_busy_step(&app_counter_3);
    }
}

/* ============================================================
 * Scenario selection
 * ============================================================ */

void app_tasks_init(void) {
#if APP_TEST_SCENARIO == APP_TEST_RR

    /*
     * Two tasks, same priority.
     * This is the cleanest Round-Robin/quantum trace.
     */
    os_task_create(task_rr_1, APP_PRIO_MID);
    os_task_create(task_rr_2, APP_PRIO_MID);

#elif APP_TEST_SCENARIO == APP_TEST_PRIORITY

    /*
     * Lower numeric means lower priority.
     * Therefore APP_PRIO_HIGH must run before APP_PRIO_LOW.
     */
    os_task_create(task_low_prio, APP_PRIO_LOW);
    os_task_create(task_high_prio, APP_PRIO_HIGH);

#elif APP_TEST_SCENARIO == APP_TEST_IDLE_DELAY

    /*
     * Both tasks repeatedly block.
     * IDLE should appear while both are BLOCKED.
     */
    os_task_create(task_delay_idle_1, APP_PRIO_MID);
    os_task_create(task_delay_idle_2, APP_PRIO_MID);

#elif APP_TEST_SCENARIO == APP_TEST_BLOCKED_DELAY

    /*
     * One task blocks repeatedly.
     * The other task should run while it is blocked.
     */
    os_task_create(task_blocked_subject, APP_PRIO_HIGH);
    os_task_create(task_background, APP_PRIO_LOW);

#elif APP_TEST_SCENARIO == APP_TEST_MIXED

    /*
     * Two high-priority tasks Round-Robin.
     * Low-priority task only runs when both high tasks are blocked.
     */
    os_task_create(task_mixed_low, APP_PRIO_LOW);
    os_task_create(task_mixed_high_1, APP_PRIO_HIGH);
    os_task_create(task_mixed_high_2, APP_PRIO_HIGH);

#else
#error "Invalid APP_TEST_SCENARIO"
#endif
}