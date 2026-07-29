#include "app_tasks.h"
#include "os_task.h"
#include "os_delay.h"
#include "os_mutex.h"
#include "os_sem.h"
#include "os_queue.h"
#include "os_types.h"
#include "stm32l4xx_hal.h"

#define BUSY_DELAY_TICKS_MIN  1u
#define BUSY_DELAY_TICKS_MAX 0x7FFFFFFF
#define BLOCK_DELAY_TICKS 25u
#define DELAY_TOLERANCE   0u
#define QUEUE_MSG_SIZE sizeof(uint32_t)
#define QUEUE_MSG_COUNT 3u

//Set Up
#define MIN_MAX_CASE        (0u)
#define RAPID_SWITCH_CASE   (0u)
#define INVALID_ARG_CASE    (0u)
#define OTHER_TASKS         (0u)

#if OTHER_TASKS 
/* Globale Kernel-Objekte für den Grenztest */
static os_mutex_t test_mutex;
static os_sem_t   test_sem;
static os_queue_t  test_queue;
static uint32_t   queue_storage[QUEUE_MSG_COUNT]; // Speicher für Queue

typedef struct {
    uint32_t sender_id;
    uint32_t sequence_num;
} queue_msg_t;

static os_queue_t struct_queue;
static queue_msg_t struct_storage[2];
#endif

volatile uint32_t test_error_count = 0u;

static void test_fail(void) {
    test_error_count++;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
}

static uint8_t delay_is_in_range(uint32_t measured, uint32_t expected) {
    return measured >= (expected - DELAY_TOLERANCE);
}

#if MIN_MAX_CASE
static void busy_delay_min_case_task(void) {
    while (1) {
        uint32_t start_tick = uwTick;

        os_delay_busy(BUSY_DELAY_TICKS_MIN);

        if (!delay_is_in_range(uwTick - start_tick, BUSY_DELAY_TICKS_MIN)) {
            test_fail();
        }

    }
}

static void busy_delay_max_case_task(void) {
    while (1) {
        uint32_t start_tick = uwTick;

        os_delay_busy(BUSY_DELAY_TICKS_MAX);

        if (!delay_is_in_range(uwTick - start_tick, BUSY_DELAY_TICKS_MAX)) {
            test_fail();
        }
    }
}
#endif

#if RAPID_SWITCH_CASE

static void busy_delay_rapid_switch_high_task(void) {
    while (1) {

        os_delay(BUSY_DELAY_TICKS_MIN);

    }
}

static void busy_delay_rapid_switch_low_task(void) {
    while (1) {
        uint32_t start_tick = uwTick;

        os_delay_busy(100u);

        if (!delay_is_in_range(uwTick - start_tick, 100u)) {
            test_fail();
        }

    }
}

#endif

#if INVALID_ARG_CASE
static void busy_delay_invalid_arg_min_task(void) {
    while (1) {

        os_status_t delay_busy = os_delay_busy(0u);

        //FAIL When return type not OS_ERR_INVALID_ARG
        if (delay_busy != OS_ERR_INVALID_ARG) {
            test_fail();
        }

    }
}

static void busy_delay_invalid_max_task(void) {
    while (1) {

        os_status_t delay_busy = os_delay_busy(0x80000001);

        //FAIL When return type not OS_ERR_INVALID_ARG
        if (delay_busy != OS_ERR_INVALID_ARG) {
            test_fail();
        }
    }
}
#endif

/*------------------------OTHER TASKS FOR STRESS TESTING------------------------------*/
#if OTHER_TASKS

static void mutex_stress_task(void) {
    while (1) {
        // 1. Mutex anfordern (kann den Task blockieren)
        if (os_mutex_lock(&test_mutex, 50u) == OS_OK) {
            
            // 2. Busy-Delay innerhalb der Critical Section ausführen
            os_delay_busy(20u);


            // 3. Mutex freigeben (kann höherprio Task aufwecken)
            os_mutex_unlock(&test_mutex);
        }

        // Kurze Pause (blockierend) zur Entlastung
        os_delay(10u);
    }
}

static void sem_worker_task(void) {
    while (1) {
        // Wartet blockierend auf ein Token
        if (os_sem_acquire(&test_sem, 100u) == OS_OK) {
            
            // Führt ein kritisches Busy-Delay aus
            os_delay_busy(30u);

            // Gibt Token wieder frei
            os_sem_release(&test_sem);
        }
        
        os_delay(5u);
    }
}

static void queue_producer_task(void) {
    uint32_t counter = 100u;

    while (1) {
        counter++;

        // Versuche Daten zu senden (blockiert max. 20 Ticks wenn voll)
        os_status_t status = os_queue_send(&test_queue, &counter, 20u);

        if (status == OS_OK) {
            // Nach erfolgreichem Senden eine kurze Busy-Phasen einlegen
            os_delay_busy(5u);
        } else if (status == OS_ERR_TIMEOUT) {
            // Timeout getestet: Queue war zu lange voll
        }

        os_delay(10u); // Freiwilliges Blockieren zur Scheduler-Entlastung
    }
}

static void queue_consumer_task(void) {
    uint32_t received_val = 0u;

    while (1) {
        // Wartet unendlich, bis der Producer eine Nachricht schickt
        os_status_t status = os_queue_recv(&test_queue, &received_val, OS_WAIT_FOREVER);

        if (status == OS_OK) {
            // Empfangene Daten verarbeiten und Busy Delay stressen
            uint32_t start_tick = uwTick;
            os_delay_busy(15u);

            if (!delay_is_in_range(uwTick - start_tick, 15u)) {
                test_fail();
            }
        }
    
    }
}

static void queue_stress_task(void) {
    queue_msg_t tx_msg = { .sender_id = 3u, .sequence_num = 0u };
    queue_msg_t rx_msg = { 0 };

    while (1) {
        tx_msg.sequence_num++;

        // 1. Non-blocking Senden (OS_NO_WAIT)
        os_status_t status = os_queue_send(&struct_queue, &tx_msg, OS_NO_WAIT);

        if (status == OS_OK) {
            // 2. Sofortiges wieder Auslesen
            if (os_queue_recv(&struct_queue, &rx_msg, OS_NO_WAIT) == OS_OK) {
                if (rx_msg.sequence_num != tx_msg.sequence_num) {
                    test_fail(); // Datenkorruption in Ring-Buffer
                }
            }
        }

        // Parallel ein langes Busy-Delay ausführen
        os_delay_busy(30u);
        os_delay(5u);
    }
}
#endif

void app_tasks_init(void) {

    #if MIN_MAX_CASE

    if (os_task_create(busy_delay_min_case_task, 2u) != OS_OK) {
        test_fail();
    }
    
    if (os_task_create(busy_delay_max_case_task, 2u) != OS_OK) {
        test_fail();
    }
    #endif

    #if RAPID_SWITCH_CASE

    if (os_task_create(busy_delay_rapid_switch_high_task, 3u) != OS_OK) {
        test_fail();
    }
    
    if (os_task_create(busy_delay_rapid_switch_low_task, 3u) != OS_OK) {
        test_fail();
    }

    #endif

    #if INVALID_ARG_CASE
    if (os_task_create(busy_delay_invalid_arg_min_task, 2u) != OS_OK) {
        test_fail();
    }
    
    if (os_task_create(busy_delay_invalid_max_task, 2u) != OS_OK) {
        test_fail();
    }
    #endif

    #if OTHER_TASKS
    os_mutex_init(&test_mutex);
    os_sem_init(&test_sem, 1u, 1u);
    
    if (os_queue_init(&struct_queue, struct_storage, sizeof(queue_msg_t), 2u) != OS_OK) {
        test_fail();
    }
    if (os_queue_init(&test_queue, queue_storage, sizeof(uint32_t), QUEUE_MSG_COUNT) != OS_OK) {
        test_fail();
    }

    
    if (os_task_create(sem_worker_task, 4u) != OS_OK) {
        test_fail();
    }

    if (os_task_create(mutex_stress_task, 3u) != OS_OK) {
        test_fail();
    }
    
    // 2. Tasks erstellen (Prio: Höhere Zahl = Höhere Priorität)
    // Consumer (Prio 4) - Wacht auf, sobald Producer schickt
    if (os_task_create(queue_consumer_task, 4u) != OS_OK) {
        test_fail();
    }

    // Producer (Prio 3) - Produziert periodisch Daten
    if (os_task_create(queue_producer_task, 3u) != OS_OK) {
        test_fail();
    }

    // Stress / Busy-Delay Task (Prio 2) - Läuft im Hintergrund
    if (os_task_create(queue_stress_task, 2u) != OS_OK) {
        test_fail();
    }
    #endif
        
}