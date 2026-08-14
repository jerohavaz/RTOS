/**
 * @file project.h
 * @brief Select the integration-test application built into the firmware.
 * @author Jerome
 *
 * @details
 * Exactly one integration test is compiled into the application at a time.
 * Change @ref PROJECT to one of the @c PROJECT_* constants below. Keeping the
 * selection in one header ensures that @c app.c and every test translation
 * unit use the same compile-time choice.
 *
 * The selection may alternatively be supplied as a compiler definition. An
 * externally defined @c PROJECT takes precedence over the default in this
 * file.
 */

#ifndef PROJECT_H_
#define PROJECT_H_

/** @brief Build the fixed-priority and round-robin scheduler test. */
#define PROJECT_SCHEDULER (1u)

/** @brief Build the blocking-delay and busy-delay test. */
#define PROJECT_DELAY (2u)

/** @brief Build the counting-semaphore contention test. */
#define PROJECT_SEMAPHORE (3u)

/** @brief Build the mutex ownership and contention test. */
#define PROJECT_MUTEX (4u)

/** @brief Build the message-queue communication and integrity test. */
#define PROJECT_QUEUE (5u)

/** @brief Build the interactive LSM6DSL sensor application. */
#define PROJECT_SENSOR (6u)

/**
 * @brief Integration test selected for the current firmware image.
 *
 * The default exercises the sensor application. Define @c PROJECT through the compiler or
 * change the value below to build another scenario.
 */
#ifndef PROJECT
#define PROJECT PROJECT_SENSOR
#endif

#if (PROJECT < PROJECT_SCHEDULER) || (PROJECT > PROJECT_SENSOR)
#error "PROJECT must select a defined integration test"
#endif

#endif /* PROJECT_H_ */
