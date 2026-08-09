#include "app_tasks.h"
#include "os_mutex.h"
#include "os_sem.h"
#include "os_task.h"
#include "stm32l4xx_hal.h"

#define WORKER_COUNT  (8u)
#define TIMEOUT_TICKS (3u)

enum { WORKER_COMMAND_WAIT, WORKER_COMMAND_TIMEOUT };

static os_mutex_t stress_mutex;
static os_mutex_t second_mutex;

static os_sem_t sem_start[WORKER_COUNT];
static os_sem_t sem_waiter_ready;
static os_sem_t sem_operation_done;

static volatile uint8_t worker_command[WORKER_COUNT];
static volatile uint8_t expected_order[WORKER_COUNT];
static volatile uint8_t handoff_position;
static volatile uint8_t mutex_occupancy;

volatile uint32_t test_error_count = 0u;
volatile uint32_t stress_round_count = 0u;
volatile uint32_t successful_lock_count = 0u;
volatile uint32_t timeout_count = 0u;
volatile uint32_t protected_update_count = 0u;
volatile uint32_t worker_lock_count[WORKER_COUNT];

static const uint8_t worker_priority[WORKER_COUNT] = { 4u, 4u, 3u, 3u, 2u, 2u, 1u, 1u };

static void test_fail(void) {
    test_error_count++;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
}

static void expect_status(os_status_t actual, os_status_t expected) {
    if (actual != expected) {
        test_fail();
    }
}

static void signal(os_sem_t *sem) {
    expect_status(os_sem_release(sem), OS_OK);
}

static void wait_for(os_sem_t *sem) {
    expect_status(os_sem_acquire(sem, OS_WAIT_FOREVER), OS_OK);
}

static void worker_run(uint8_t worker_id) {
    while (1) {
        wait_for(&sem_start[worker_id]);

        if (worker_command[worker_id] == WORKER_COMMAND_TIMEOUT) {
            /*
             * The controller owns stress_mutex throughout this probe.
             * Exercise three rejected acquisition/ownership paths.
             */
            expect_status(os_mutex_unlock(&stress_mutex), OS_ERR_NOT_OWNER);

            expect_status(os_mutex_lock(&stress_mutex, OS_NO_WAIT), OS_ERR_WOULD_BLOCK);

            expect_status(os_mutex_lock(&stress_mutex, TIMEOUT_TICKS), OS_ERR_TIMEOUT);

            timeout_count++;
            signal(&sem_operation_done);
            continue;
        }

        /*
         * All workers have a higher priority than the controller. Releasing
         * sem_waiter_ready makes the controller ready, but it cannot execute
         * until this worker has entered the mutex wait queue and blocked.
         */
        signal(&sem_waiter_ready);

        expect_status(os_mutex_lock(&stress_mutex, OS_WAIT_FOREVER), OS_OK);

        /*
         * Check the complete handoff order independently of the trace
         * verifier.
         */
        if (handoff_position >= WORKER_COUNT) {
            test_fail();
        } else if (expected_order[handoff_position] != worker_id) {
            test_fail();
        }

        handoff_position++;

        /*
         * Detect simultaneous critical-section entry independently of mutex
         * ownership fields and trace events.
         */
        if (mutex_occupancy != 0u) {
            test_fail();
        }

        mutex_occupancy++;

        protected_update_count++;
        worker_lock_count[worker_id]++;
        successful_lock_count++;

        /*
         * A worker receiving ownership through direct handoff must be treated
         * as the real owner. Recursive acquisition must still be rejected.
         */
        expect_status(os_mutex_lock(&stress_mutex, OS_NO_WAIT), OS_ERR_INVALID_STATE);

        /*
         * Make the critical section nontrivial and increase the chance of tick
         * interrupts occurring while the mutex is owned.
         */
        for (volatile uint32_t spin = 0u; spin < 2000u; spin++) {
        }

        if (mutex_occupancy != 1u) {
            test_fail();
        }

        mutex_occupancy--;

        expect_status(os_mutex_unlock(&stress_mutex), OS_OK);
        signal(&sem_operation_done);
    }
}

static void worker_0_task(void) {
    worker_run(0u);
}

static void worker_1_task(void) {
    worker_run(1u);
}

static void worker_2_task(void) {
    worker_run(2u);
}

static void worker_3_task(void) {
    worker_run(3u);
}

static void worker_4_task(void) {
    worker_run(4u);
}

static void worker_5_task(void) {
    worker_run(5u);
}

static void worker_6_task(void) {
    worker_run(6u);
}

static void worker_7_task(void) {
    worker_run(7u);
}

/**
 * @brief Construct the required priority/FIFO ownership order.
 *
 * Higher numeric task priority wins. Workers with equal priority retain their
 * insertion order.
 *
 * @param reverse Nonzero when workers are inserted from 7 down to 0.
 */
static void build_expected_order(uint8_t reverse) {
    uint8_t output = 0u;

    for (uint8_t priority = 4u; priority > 0u; priority--) {
        for (uint8_t position = 0u; position < WORKER_COUNT; position++) {
            uint8_t worker_id;

            if (reverse != 0u) {
                worker_id = (uint8_t)(WORKER_COUNT - 1u - position);
            } else {
                worker_id = position;
            }

            if (worker_priority[worker_id] == priority) {
                expected_order[output] = worker_id;
                output++;
            }
        }
    }

    if (output != WORKER_COUNT) {
        test_fail();
    }
}

static void controller_task(void) {
    /*
     * Exercise a second mutex address to detect accidental cross-instance
     * monitor state.
     */
    expect_status(os_mutex_lock(&second_mutex, OS_NO_WAIT), OS_OK);

    expect_status(os_mutex_lock(&second_mutex, OS_NO_WAIT), OS_ERR_INVALID_STATE);

    expect_status(os_mutex_unlock(&second_mutex), OS_OK);

    /*
     * The controller retains stress_mutex while workers are inserted into its
     * wait queue.
     */
    expect_status(os_mutex_lock(&stress_mutex, OS_NO_WAIT), OS_OK);

    expect_status(os_mutex_lock(&stress_mutex, OS_NO_WAIT), OS_ERR_INVALID_STATE);

    while (1) {
        uint32_t next_round = stress_round_count + 1u;
        uint8_t reverse = (uint8_t)(next_round & 1u);
        uint8_t timeout_worker = (uint8_t)(next_round % WORKER_COUNT);

        /*
         * Rotate the timeout probe through all workers. The same worker later
         * joins the full wait queue, testing whether timeout cleanup removed
         * every stale wait state.
         */
        worker_command[timeout_worker] = WORKER_COMMAND_TIMEOUT;
        signal(&sem_start[timeout_worker]);
        wait_for(&sem_operation_done);
        worker_command[timeout_worker] = WORKER_COMMAND_WAIT;

        if (timeout_count != next_round) {
            test_fail();
        }

        build_expected_order(reverse);
        handoff_position = 0u;

        /*
         * Saturate the wait queue. Alternate ascending and descending
         * insertion order so FIFO behavior cannot accidentally pass because
         * of fixed task IDs.
         */
        for (uint8_t position = 0u; position < WORKER_COUNT; position++) {
            uint8_t worker_id;

            if (reverse != 0u) {
                worker_id = (uint8_t)(WORKER_COUNT - 1u - position);
            } else {
                worker_id = position;
            }

            signal(&sem_start[worker_id]);
            wait_for(&sem_waiter_ready);
        }

        /*
         * Begin an eight-owner direct-handoff chain. Expected selection:
         *
         * priority 4, FIFO
         * priority 3, FIFO
         * priority 2, FIFO
         * priority 1, FIFO
         */
        expect_status(os_mutex_unlock(&stress_mutex), OS_OK);

        for (uint8_t completed = 0u; completed < WORKER_COUNT; completed++) {
            wait_for(&sem_operation_done);
        }

        if (handoff_position != WORKER_COUNT) {
            test_fail();
        }

        if (mutex_occupancy != 0u) {
            test_fail();
        }

        if (successful_lock_count != next_round * WORKER_COUNT) {
            test_fail();
        }

        if (protected_update_count != next_round * WORKER_COUNT) {
            test_fail();
        }

        for (uint8_t worker_id = 0u; worker_id < WORKER_COUNT; worker_id++) {
            if (worker_lock_count[worker_id] != next_round) {
                test_fail();
            }
        }

        stress_round_count = next_round;

        /*
         * The final worker released without a successor. The mutex must now be
         * free and immediately acquirable by the controller.
         */
        expect_status(os_mutex_lock(&stress_mutex, OS_NO_WAIT), OS_OK);

        expect_status(os_mutex_lock(&stress_mutex, OS_NO_WAIT), OS_ERR_INVALID_STATE);
    }
}

void app_tasks_init(void) {
    static void (*const worker_task[WORKER_COUNT])(
        void) = { worker_0_task, worker_1_task, worker_2_task, worker_3_task,
                  worker_4_task, worker_5_task, worker_6_task, worker_7_task };

    expect_status(os_mutex_init(0), OS_ERR_NULL);
    expect_status(os_mutex_init(&stress_mutex), OS_OK);
    expect_status(os_mutex_init(&second_mutex), OS_OK);

    for (uint8_t worker_id = 0u; worker_id < WORKER_COUNT; worker_id++) {
        worker_command[worker_id] = WORKER_COMMAND_WAIT;
        worker_lock_count[worker_id] = 0u;

        expect_status(os_sem_init(&sem_start[worker_id], 0u, 1u), OS_OK);
    }

    expect_status(os_sem_init(&sem_waiter_ready, 0u, 1u), OS_OK);

    expect_status(os_sem_init(&sem_operation_done, 0u, WORKER_COUNT), OS_OK);

    expect_status(os_mutex_lock(0, OS_NO_WAIT), OS_ERR_NULL);
    expect_status(os_mutex_unlock(0), OS_ERR_NULL);

    for (uint8_t worker_id = 0u; worker_id < WORKER_COUNT; worker_id++) {
        expect_status(os_task_create(worker_task[worker_id], worker_priority[worker_id]), OS_OK);
    }

    /*
     * Priority zero ensures every released worker runs and blocks before the
     * controller resumes.
     */
    expect_status(os_task_create(controller_task, 0u), OS_OK);
}