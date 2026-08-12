/**
 * @file app.h
 * @brief Selected integration-test application entry point.
 * @author Jerome
 *
 * Dispatches initialization to the scenario selected by @c PROJECT.
 */

#ifndef APP_H_
#define APP_H_

/**
 * @brief Initialize the integration test selected by @c PROJECT.
 *
 * Resets the shared result and initializes exactly one component test.
 *
 * @pre The kernel has been initialized with @c os_init().
 * @pre The scheduler has not yet been started with @c os_start().
 * @post The shared result is running or failed if setup detected an error.
 */
void app_init(void);

#endif /* APP_H_ */
