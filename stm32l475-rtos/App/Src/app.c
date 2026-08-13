/**
 * @file app.c
 * @brief Integration-test selection and shared result implementation.
 * @author Jerome
 *
 * @details
 * Implements the common assertion result and dispatches to exactly one test
 * initializer at compile time. No runtime selection or unused test task is
 * introduced into the firmware image.
 */

#include "app.h"
#include "integration_test.h"
#include "integration_tests.h"
#include "project.h"

/** @brief Shared debugger-visible verdict for the selected integration test. */
integration_test_result_t g_integration_test_result = {
    .state = INTEGRATION_TEST_NOT_STARTED,
    .checks = 0u,
    .failures = 0u,
};

void integration_test_begin(void) {
    g_integration_test_result.checks = 0u;
    g_integration_test_result.failures = 0u;
    g_integration_test_result.state = INTEGRATION_TEST_RUNNING;
}

void integration_test_check(bool condition) {
    g_integration_test_result.checks++;

    if (!condition) {
        g_integration_test_result.failures++;
        g_integration_test_result.state = INTEGRATION_TEST_FAILED;
    }
}

void integration_test_pass(void) {
    if (g_integration_test_result.failures == 0u) {
        g_integration_test_result.state = INTEGRATION_TEST_PASSED;
    }
}

void app_init(void) {
    integration_test_begin();

#if PROJECT == PROJECT_SCHEDULER
    integration_scheduler_init();
#elif PROJECT == PROJECT_DELAY
    integration_delay_init();
#elif PROJECT == PROJECT_SEMAPHORE
    integration_semaphore_init();
#elif PROJECT == PROJECT_MUTEX
    integration_mutex_init();
#elif PROJECT == PROJECT_QUEUE
    integration_queue_init();
#elif PROJECT == PROJECT_SENSOR
    integration_sensor_app_init();
#else
#error "Unsupported PROJECT selection"
#endif
}
