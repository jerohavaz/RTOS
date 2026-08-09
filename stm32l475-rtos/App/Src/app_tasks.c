#include "app_tasks.h"
#include "os_task.h"
#include "os_delay.h"
#include "os_mutex.h"
#include "os_sem.h"
#include "os_queue.h"
#include "os_types.h"
#include "stm32l4xx_hal.h"

/* ==========================================================================
 * Test Feature Switches (0u = Deaktiviert, 1u = Aktiviert)
 * ========================================================================== */
#define TEST_BUSY_DELAY_ENABLE         (1u) // PASS
#define TEST_NON_BLOCKING_DELAY_ENABLE (1u) // PASS
#define TEST_SEMAPHORE_DELAY_ENABLE    (1u) // PASS
#define TEST_MUTEX_DELAY_ENABLE        (1u) // PASS
#define TEST_QUEUE_DELAY_ENABLE        (1u) // PASS

/* ==========================================================================
 * Configuration & Delay Defines
 * ========================================================================== */

/* Priorities */
#define PRIO_HIGH   4u
#define PRIO_MEDIUM 3u
#define PRIO_LOW    2u

/* Delays & Timeouts in Ticks */
#define BUSY_DELAY_TICKS            30u
#define STANDARD_DELAY_TICKS        50u
#define SEM_TIMEOUT_TICKS           20u
#define SEM_RELEASE_INTERVAL_TICKS  40u
#define MUTEX_HOLD_DELAY_TICKS      35u
#define MUTEX_LOCK_TIMEOUT_TICKS    15u
#define QUEUE_PRODUCE_DELAY_TICKS   25u
#define QUEUE_CONSUME_TIMEOUT_TICKS 10u

/* Object Configurations */
#define SEM_INITIAL_COUNT 0u
#define SEM_MAX_COUNT     1u
#define QUEUE_MSG_COUNT   2u

/* Global Error Tracking */
volatile uint32_t test_error_count = 0u;

static inline void test_fail(void) {
    test_error_count++;
}

/* ==========================================================================
 * Global OS Objects (Conditional Compilation)
 * ========================================================================== */

#if TEST_SEMAPHORE_DELAY_ENABLE
static os_sem_t test_sem;
#endif

#if TEST_MUTEX_DELAY_ENABLE
static os_mutex_t test_mutex;
#endif

#if TEST_QUEUE_DELAY_ENABLE
typedef struct {
    uint32_t sender_id;
    uint32_t payload;
} queue_msg_t;

static os_queue_t test_queue;
static queue_msg_t queue_storage[QUEUE_MSG_COUNT];
#endif

/* ==========================================================================
 * 1. Busy Delay Task (Aktives Warten)
 * ========================================================================== */
#if TEST_BUSY_DELAY_ENABLE
static void busy_delay_task(void) {
    while (1) {
        /* os_delay_busy blockiert die CPU aktiv und bleibt im Zustand RUNNING */
        os_status_t status = os_delay_busy(BUSY_DELAY_TICKS);
        if (status != OS_OK) {
            test_fail();
        }

        /* Kurzes Standard-Delay, um anderen Tasks Rechenzeit zu geben */
        os_delay(STANDARD_DELAY_TICKS);
    }
}
#endif

/* ==========================================================================
 * 2. Standard Non-Blocking Delay Task
 * ========================================================================== */
#if TEST_NON_BLOCKING_DELAY_ENABLE
static void non_blocking_delay_task(void) {
    while (1) {
        /* os_delay versetzt den Task in den Zustand BLOCKED */
        os_status_t status = os_delay(1u);
        if (status != OS_OK) {
            test_fail();
        }
    }
}
#endif

/* ==========================================================================
 * 3. Semaphore Delay & Timeout Tasks
 * ========================================================================== */
#if TEST_SEMAPHORE_DELAY_ENABLE
static void sem_delay_task(void) {
    while (1) {
        /* Versuche Semaphore mit Timeout zu erwerben. */
        os_status_t status = os_sem_acquire(&test_sem, SEM_TIMEOUT_TICKS);

        if (status == OS_ERR_TIMEOUT) {
            /* Erwarteter Timeout-Fall */
        } else if (status == OS_OK) {
            /* Erfolgreich erworben */
        } else {
            test_fail();
        }

        os_delay(STANDARD_DELAY_TICKS);
    }
}

static void sem_releaser_task(void) {
    while (1) {
        os_delay(SEM_RELEASE_INTERVAL_TICKS);
        os_sem_release(&test_sem);
    }
}
#endif

/* ==========================================================================
 * 4. Mutex Delay & Lock Timeout Tasks
 * ========================================================================== */
#if TEST_MUTEX_DELAY_ENABLE
static void mutex_holder_task(void) {
    while (1) {
        if (os_mutex_lock(&test_mutex, OS_WAIT_FOREVER) == OS_OK) {
            /* Halte den Mutex während eines Delays */
            os_delay(MUTEX_HOLD_DELAY_TICKS);
            os_mutex_unlock(&test_mutex);
        } else {
            test_fail();
        }

        os_delay(STANDARD_DELAY_TICKS);
    }
}

static void mutex_delay_task(void) {
    while (1) {
        /* Versuche den Mutex zu sperren, während der Holder ihn noch hält */
        os_status_t status = os_mutex_lock(&test_mutex, MUTEX_LOCK_TIMEOUT_TICKS);

        if (status == OS_ERR_TIMEOUT) {
            /* Erwartetes Verhalten: Mutex war belegt */
        } else if (status == OS_OK) {
            os_mutex_unlock(&test_mutex);
        } else {
            test_fail();
        }

        os_delay(STANDARD_DELAY_TICKS);
    }
}
#endif

/* ==========================================================================
 * 5. Message Queue Delay Tasks (Producer & Consumer)
 * ========================================================================== */
#if TEST_QUEUE_DELAY_ENABLE
static void queue_delay_producer_task(void) {
    uint32_t seq = 0;

    while (1) {
        queue_msg_t msg = { .sender_id = 1u, .payload = seq++ };

        /* Sende Nachricht in die Queue. Wenn voll, warte maximal QUEUE_PRODUCE_DELAY_TICKS */
        os_status_t status = os_queue_send(&test_queue, &msg, QUEUE_PRODUCE_DELAY_TICKS);
        if (status != OS_OK && status != OS_ERR_TIMEOUT) {
            test_fail();
        }

        os_delay(QUEUE_PRODUCE_DELAY_TICKS);
    }
}

static void queue_delay_consumer_task(void) {
    while (1) {
        queue_msg_t msg;

        /* Empfange mit kurzem Timeout */
        os_status_t status = os_queue_recv(&test_queue, &msg, QUEUE_CONSUME_TIMEOUT_TICKS);
        if (status != OS_OK && status != OS_ERR_TIMEOUT) {
            test_fail();
        }

        /* Teste busy_delay im Consumer nach dem Empfang */
        os_delay_busy(BUSY_DELAY_TICKS);
    }
}
#endif

/* ==========================================================================
 * Application Setup / Init
 * ========================================================================== */
void app_tasks_init(void) {
    /* --- 1. Busy Delay Task Init --- */
#if TEST_BUSY_DELAY_ENABLE
    if (os_task_create(busy_delay_task, PRIO_MEDIUM) != OS_OK) {
        test_fail();
    }
#endif

    /* --- 2. Non-Blocking Delay Task Init --- */
#if TEST_NON_BLOCKING_DELAY_ENABLE
    if (os_task_create(non_blocking_delay_task, PRIO_LOW) != OS_OK) {
        test_fail();
    }
#endif

    /* --- 3. Semaphore Tasks Init --- */
#if TEST_SEMAPHORE_DELAY_ENABLE
    if (os_sem_init(&test_sem, SEM_INITIAL_COUNT, SEM_MAX_COUNT) != OS_OK) {
        test_fail();
    }

    if (os_task_create(sem_delay_task, PRIO_HIGH) != OS_OK) {
        test_fail();
    }

    if (os_task_create(sem_releaser_task, PRIO_MEDIUM) != OS_OK) {
        test_fail();
    }
#endif

    /* --- 4. Mutex Tasks Init --- */
#if TEST_MUTEX_DELAY_ENABLE
    if (os_mutex_init(&test_mutex) != OS_OK) {
        test_fail();
    }

    if (os_task_create(mutex_holder_task, PRIO_MEDIUM) != OS_OK) {
        test_fail();
    }

    if (os_task_create(mutex_delay_task, PRIO_HIGH) != OS_OK) {
        test_fail();
    }
#endif

    /* --- 5. Queue Tasks Init --- */
#if TEST_QUEUE_DELAY_ENABLE
    if (os_queue_init(&test_queue, 1, queue_storage, sizeof(queue_msg_t), QUEUE_MSG_COUNT) !=
        OS_OK) {
        test_fail();
    }

    if (os_task_create(queue_delay_producer_task, PRIO_LOW) != OS_OK) {
        test_fail();
    }

    if (os_task_create(queue_delay_consumer_task, PRIO_MEDIUM) != OS_OK) {
        test_fail();
    }
#endif
}