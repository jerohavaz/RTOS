#include <stdint.h>
#include "tcb.h"
#include "prio_waitq.h"
#include "port.h"
#include "k_sched.h"
#define MAX_TOTAL_TASKS 5


typedef enum{
    MutexState_Locked,
    MutexState_Unlocked,
} eMutexState;



typedef struct{
    uint8_t u8SemID;
    uint32_t u32CountVal; //Ob Counting oder Binär erschließt sich aus Max Value
    uint32_t u32MaxVal;

    prio_waitq_t wait_queue;
} sctSemaphore_t;

typedef struct{
    uint8_t u8MutID;
    eMutexState eMutState;
    kernel_task_t *pOwner;
    uint8_t u8RecCount;
    prio_waitq_t wait_queue;

} sctMutex_t;


void create_semaphore(sctSemaphore_t *sem, 
                        uint8_t ID, 
                        uint32_t initVal, 
                        uint32_t maxVal, 
                        kernel_task_t *currentTask);

void sem_aquire(sctSemaphore_t *sem, kernel_task_t *currentTask, uint32_t TimeoutTicks);
void sem_release(sctSemaphore_t *sem);

void create_mutex(sctMutex_t *mut, uint8_t ID, eMutexState initState);
void mut_aquire(sctMutex_t *mut, kernel_task_t *currentTask, uint32_t TimeoutTicks);
void mut_release(sctMutex_t *mut, kernel_task_t *currentTask);