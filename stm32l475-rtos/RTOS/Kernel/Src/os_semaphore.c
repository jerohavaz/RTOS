#include "os_semaphore.h"
#include "kernel_task.h"
#include <stdint.h>

static kernel_task_list_node_t *sched_node(kernel_task_t *task) {
    return &task->sched_node;
}

void create_semaphore(sctSemaphore_t *sem, uint8_t ID, eSemaphoreState semState, eSemaphoreType semType, uint32_t initVal, uint32_t maxVal, kernel_task_t *currentTask){
    sem->u8SemID = ID;
    sem->u32CountVal = initVal;
    sem->u32MaxVal = maxVal;

    prio_waitq_init(&(sem->wait_queue), sched_node); 

}



/*-----------Semaphore Beginn--------------------------*/
void sem_aquire(sctSemaphore_t *sem, kernel_task_t *currentTask, uint32_t TimeoutTicks){
    uint32_t key = port_enter_critical();

    //Abfrage für Binär
    if(sem->u32MaxVal <= 1 && sem->u32CountVal == 1){

        sem->u32CountVal--;
        port_exit_critical(key);
        return;

    } 
    //Abfrage für Counting
    else if(sem->u32CountVal > 0){
        sem->u32CountVal--;
        port_exit_critical(key);
        return;
    }

    //Semaphore nicht verfügbar -> Task auf die wait_queue
    k_sched_task_block(currentTask);
    prio_waitq_push(&sem->wait_queue, currentTask);
    k_sched_request_switch();
    port_exit_critical(key);
}

void sem_release(sctSemaphore_t *sem){
    uin32_t key = port_enter_critical();

    if(!prio_waitq_is_empty(&sem->wait_queue)){
        kernel_task_t *nextTask = prio_waitq_pop_highest(&sem->wait_queue);
        k_sched_task_ready(nextTask);
        //nextTask->tcb.eTaskState = TaskState_Ready;
    }
    else{
        sem->u32CountVal++;
    }
    k_sched_request_switch();
    port_exit_critical(key);
    
}
/*-----------Semaphore Ende--------------------------*/


/*-----------Mutex Beginn--------------------------*/
void create_mutex(sctMutex_t *mut, uint8_t ID, eMutexState initState){
    mut->u8MutID = ID;
    mut->eMutState = initState;
    mut->pOwner = 0;
    mut->u8RecCount = 0;
}

void mut_aquire(sctMutex_t *mut, kernel_task_t *currentTask, uint32_t TimeoutTicks){
    uint32_t key = port_enter_critical();

    if(mut->pOwner == 0){
        mut->pOwner = currentTask;
        mut->eMutState = MutexState_Locked;
        port_exit_critical(key);
        return;
    }


    k_sched_task_block(currentTask);
    prio_waitq_push(&mut->wait_queue, currentTask);
    k_sched_request_switch();
    port_exit_critical(key);

}

void mut_release(sctMutex_t *mut, kernel_task_t *currentTask){
    uint32_t key = port_enter_critical();

    if(mut->pOwner != currentTask){
        port_exit_critical(key);
        return;
    }


    if(!prio_waitq_is_empty(&mut->wait_queue)){
        kernel_task_t *nextTask = prio_waitq_pop_highest(&mut->wait_queue);
        
        k_sched_task_ready(nextTask);
        mut->pOwner = nextTask;
    }
    else{
        mut->eMutState = MutexState_Unlocked;
        mut->pOwner = 0;
    }

    k_sched_request_switch();
    port_exit_critical(key);

}
/*-----------Mutex Ende--------------------------*/
