/**
 * @file integration_tests.h
 * @brief Initialization entry points for selectable integration tests.
 * @author Jerome
 *
 * @details
 * These functions create the objects and tasks required by their scenario.
 * Only the function selected by @c PROJECT is referenced by the application.
 * Each function must run after kernel initialization and before scheduler
 * startup because task creation is locked by @c os_start().
 */

#ifndef INTEGRATION_TESTS_H_
#define INTEGRATION_TESTS_H_

/**
 * @brief Create tasks for the scheduler integration test.
 * @pre @c os_init() has completed and @c os_start() has not been called.
 */
void integration_scheduler_init(void);

/**
 * @brief Create tasks for the delay integration test.
 * @pre @c os_init() has completed and @c os_start() has not been called.
 */
void integration_delay_init(void);

/**
 * @brief Initialize objects and create tasks for the semaphore test.
 * @pre @c os_init() has completed and @c os_start() has not been called.
 */
void integration_semaphore_init(void);

/**
 * @brief Initialize the mutex and create its integration-test tasks.
 * @pre @c os_init() has completed and @c os_start() has not been called.
 */
void integration_mutex_init(void);

/**
 * @brief Initialize the queue and create its integration-test tasks.
 * @pre @c os_init() has completed and @c os_start() has not been called.
 */
void integration_queue_init(void);

/**
 * @brief Initialize the LSM6DSL sensor application and its worker tasks.
 * @pre @c os_init() has completed and @c os_start() has not been called.
 */
void integration_sensor_app_init(void);

#endif /* INTEGRATION_TESTS_H_ */
