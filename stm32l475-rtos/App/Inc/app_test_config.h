#ifndef APP_TEST_CONFIG_H
#define APP_TEST_CONFIG_H

#include <stdint.h>

/*
 * Priority convention for this project:
 * lower numeric value  => lower priority
 * higher numeric value => higher priority
 */
#define APP_PRIO_LOW      (1u)
#define APP_PRIO_MID      (2u)
#define APP_PRIO_HIGH     (3u)

/*
 * Scenario selector.
 * Build/run one scenario, capture RTT trace, convert to TeSSLa input,
 * then run scheduler.generated.tessla on that trace.
 */
#define APP_TEST_RR              (1u)
#define APP_TEST_PRIORITY        (2u)
#define APP_TEST_IDLE_DELAY      (3u)
#define APP_TEST_BLOCKED_DELAY   (4u)
#define APP_TEST_MIXED           (5u)

/*
 * Select active scenario here.
 */
#ifndef APP_TEST_SCENARIO
#define APP_TEST_SCENARIO APP_TEST_BLOCKED_DELAY
#endif

/*
 * Keep quantum small enough that traces are readable.
 * This should match your RTOS scheduler quantum config.
 */
#define APP_EXPECTED_QUANTUM_TICKS (10u)

/*
 * Delay durations for blocking/non-blocking delay tests.
 * Use values clearly larger than the quantum.
 */
#define APP_SHORT_DELAY_TICKS      (20u)
#define APP_LONG_DELAY_TICKS       (100u)

#endif