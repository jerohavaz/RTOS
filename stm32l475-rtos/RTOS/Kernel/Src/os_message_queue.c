#include "os_message_queue.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "k_timeout.h"
#include "k_sched.h"
#include "kernel_task.h"

static kernel_task_list_node_t *sched_node(kernel_task_t *task) {
    return &task->sched_node;
}

 void Queue_Create(QCB_sctQCB_t *qcb, const char *QName, uint8_t QID, uint8_t QLength, uint8_t MLength, kernel_task_t *currentTask){
    
    //String Kopieren
    strncpy(qcb->sQName, QName, sizeof(qcb->sQName) -1);
    qcb->sQName[sizeof(qcb->sQName) -1] = '\0';    //Terminatorsymbol als Ende des STrings

    qcb->u8QID = QID;
    qcb->u8QLength = QLength;
    qcb->u8MessageLength = MLength;

    qcb->uReadIndex = 0;
    qcb->uWriteIndex = 0;
    qcb->uMessageCount = 0;

    qcb->pBuffer = malloc((size_t)QLength * MLength);
    prio_waitq_init(&(qcb->send_queue), sched_node);
    prio_waitq_init(&(qcb->receive_queue), sched_node);


    return;
}

/*----------------------Senden----------------------------*/
void Queue_Send(QCB_sctQCB_t *qcb, void *payload, uint32_t TimeoutTicks, kernel_task_t *currentTask){
    uint32_t key = port_enter_critical();

    if(qcb->uMessageCount == qcb->u8QLength){

        //Non Blocking
        if(TimeoutTicks == 0){
            port_exit_critical(key);
            return;
        }

        //Block with Timeout
        else if (TimeoutTicks > 0) {
            k_sched_task_block(currentTask);
            prio_waitq_push(&(qcb->send_queue), currentTask);
                    
            //Wait Objekt wird übergeben
            currentTask->wait_type = K_WAIT_QUEUE_SEND;
            currentTask->wait_object = qcb;
            //Timeout Node zuweisung
            k_timeout_add(currentTask, TimeoutTicks);
            
            k_sched_request_switch();
            port_exit_critical(key);

            return;
        }

        //Block forever
        else if(TimeoutTicks == WAIT_FOREVER){
            k_sched_task_block(currentTask);
            prio_waitq_push(&qcb->send_queue, currentTask);

            k_sched_request_switch();
            port_exit_critical(key);

            return;

        }
    }

    //Senden
    uint8_t *slot = qcb->pBuffer + (qcb->uWriteIndex * qcb->u8MessageLength);
    memcpy(slot, &payload, qcb->u8MessageLength);

    qcb->uWriteIndex = (qcb->uWriteIndex + 1) % qcb->u8QLength; 
    qcb->uMessageCount++;

    k_sched_request_switch();

    port_exit_critical(key);

    return;
}


/*-----------------------------------Empfangen-----------------------*/
void Queue_Receive(QCB_sctQCB_t *qcb, void *ReceivePuffer, uint32_t TimeoutTicks, kernel_task_t *currentTask){
    uint32_t key = port_enter_critical();

    if(qcb->uMessageCount == 0){

        //Non-Blocking
        if(TimeoutTicks == 0){
            port_exit_critical(key);
            return;
        }

        //Block with Timeout
        if(TimeoutTicks > 0){
            k_sched_task_block(currentTask);
            prio_waitq_push(&(qcb->receive_queue), currentTask);
            
            //Wait Objekt wird übergeben
            currentTask->wait_type = K_WAIT_QUEUE_RECV;
            currentTask->wait_object = qcb;

            //Timeout
            k_timeout_add(currentTask, TimeoutTicks);
            
            k_sched_request_switch();
            port_exit_critical(key);
            
            return;

        }

        //Block Forever
        if(TimeoutTicks == WAIT_FOREVER){

            k_sched_task_block(currentTask);
            prio_waitq_push(&qcb->receive_queue, currentTask);

            k_sched_request_switch();
            port_exit_critical(key);

            return;
        }
    }


    //Empfangen
    uint8_t *slot = qcb->pBuffer + (qcb->uReadIndex * qcb->u8MessageLength);
    memcpy(ReceivePuffer, slot, qcb->u8MessageLength);

    qcb->uReadIndex = (qcb->uReadIndex + 1) % qcb->u8QLength;
    qcb->uMessageCount--;

    k_sched_request_switch();

    port_exit_critical(key);

    return;

}
