/**
 * @file integration_test.h
 * @brief Shared integration-test result reporting.
 * @author Jerome
 *
 * @details
 * Every selectable scenario reports through one debugger-visible result. The
 * counters record evaluated assertions, while the state provides a compact
 * verdict. Component-specific observation structures provide the detailed
 * execution evidence.
 */

#ifndef INTEGRATION_TEST_H_
#define INTEGRATION_TEST_H_

#include <stdbool.h>
#include <stdint.h>

/** @brief Overall state of the selected integration test. */
typedef enum {
    INTEGRATION_TEST_NOT_STARTED = 0u, /**< Application test initialization has not run. */
    INTEGRATION_TEST_RUNNING,          /**< The selected scenario is still executing. */
    INTEGRATION_TEST_PASSED,           /**< All required interactions completed without failure. */
    INTEGRATION_TEST_FAILED            /**< At least one evaluated condition failed. */
} integration_test_state_t;

/**
 * @brief Debugger-visible result of the selected integration test.
 *
 * A failed check makes @ref state sticky through @ref failures: a later call
 * to integration_test_pass() cannot convert a result with failures to passed.
 */
typedef struct {
    volatile integration_test_state_t state; /**< Current aggregate test state. */
    volatile uint32_t checks;                /**< Number of evaluated conditions. */
    volatile uint32_t failures;              /**< Number of conditions that evaluated false. */
} integration_test_result_t;

/**
 * @brief Aggregate result inspected during or after a test run.
 *
 * @note Volatile makes debugger and cross-task updates observable; it does not
 *       provide inter-core synchronization. The target is single-core and the
 *       test scenarios serialize verdict-changing operations through their
 *       priority and blocking structure.
 */
extern integration_test_result_t g_integration_test_result;

/**
 * @brief Reset the shared result and mark the selected test as running.
 *
 * @post The check and failure counters are zero.
 * @post The state is @ref INTEGRATION_TEST_RUNNING.
 */
void integration_test_begin(void);

/**
 * @brief Record one integration-test condition.
 *
 * @param condition Condition evaluated by the current test task.
 *
 * @post The check counter is incremented once.
 * @post A false condition increments the failure counter and changes the state
 *       to @ref INTEGRATION_TEST_FAILED.
 */
void integration_test_check(bool condition);

/**
 * @brief Complete the selected integration test.
 *
 * Changes the state to @ref INTEGRATION_TEST_PASSED only when the failure
 * counter is zero. An existing failure verdict is preserved.
 */
void integration_test_pass(void);

#endif /* INTEGRATION_TEST_H_ */
