#include "os_message_queue.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "k_timeout.h"
#include "k_sched.h"
#include "kernel_task.h"
#include "os_types.h"
#include "port.h"
#include "prio_waitq.h"
#include "timeout_list.h"
#include "kernel_panic.h"

static kernel_task_list_node_t *sched_node(kernel_task_t *task) {
    return &task->sched_node;
}

void os_queue_create(QCB_sctQCB_t *qcb, const char *name, uint8_t id, void *buffer, uint32_t msg_len, uint32_t q_length) {
    
    KERNEL_REQUIRE(qcb != 0);
    KERNEL_REQUIRE(name != 0);
    KERNEL_REQUIRE(buffer != 0);
    KERNEL_REQUIRE(msg_len > 0);
    KERNEL_REQUIRE(q_length > 0);
    
    
    // String Kopieren
    strncpy(qcb->name, name, sizeof(qcb->name) - 1);
    qcb->name[sizeof(qcb->name) - 1] = '\0'; // Terminatorsymbol als Ende des STrings

    qcb->id = id;
    
    ringbuffer_init(&qcb->ring_buf, buffer, msg_len, q_length);

    prio_waitq_init(&(qcb->send_waitq), sched_node);
    prio_waitq_init(&(qcb->receive_waitq), sched_node);

    return;
}
// TODO: Kernel Require(cond) am ANfang send und receive für po task
// Fehlererkennung
/*----------------------Senden----------------------------*/
os_status_t os_queue_send(QCB_sctQCB_t *qcb,
                            const void *payload,
                            uint32_t timeout_ticks) {
    
    KERNEL_REQUIRE(qcb != 0);
    KERNEL_REQUIRE(payload != 0);
    
    
    uint32_t key = port_enter_critical();

    kernel_task_t *current_task = k_sched_current();

    KERNEL_REQUIRE(current_task != 0);

    //1. Wenn receive_waitq nicht leer -> Direkte übergabe
    if(!prio_waitq_is_empty(&qcb->receive_waitq) && ringbuffer_is_empty(&qcb->ring_buf)){
        
        kernel_task_t *receive_node = prio_waitq_pop_highest(&qcb->receive_waitq);
        k_timeout_remove(receive_node);
        
        receive_node->wait_object = 0;
        receive_node->wait_type = K_WAIT_NONE;
        receive_node->wake_tick = 0;
        receive_node->wait_result = OS_OK;
        
        //Direkte Übergabe
        KERNEL_REQUIRE(receive_node->wait_data != 0);

        memcpy(receive_node->wait_data, payload,  qcb->ring_buf.content_size);
        
        k_sched_task_ready(receive_node);
        k_sched_request_switch();


        port_exit_critical(key);

        return OS_OK;
    }


    //2. Wenn receive_wait leer und msgq voll
    if(ringbuffer_is_full(&qcb->ring_buf)){
        // Non Blocking
        if (timeout_ticks == 0) {
            port_exit_critical(key);
            return OS_ERR_FULL;
        }

        // Block forever
        else if (timeout_ticks == OS_WAIT_FOREVER) {
            k_sched_task_block(current_task);
            prio_waitq_push(&qcb->send_waitq, current_task);

            current_task->wait_object = qcb;
            current_task->wait_type = K_WAIT_QUEUE_SEND;
            current_task->wait_data = (void*) payload;

        }

        // Block with Timeout
        else if (timeout_ticks != OS_WAIT_FOREVER && timeout_ticks > 0) {
            k_sched_task_block(current_task);
            prio_waitq_push(&(qcb->send_waitq), current_task);

            // Wait Objekt wird übergeben
            current_task->wait_type = K_WAIT_QUEUE_SEND;
            current_task->wait_object = qcb;
            current_task->wait_data = (void*) payload;
            // Timeout Node zuweisung
            k_timeout_add(current_task, timeout_ticks);
        }
        k_sched_request_switch();

        
        port_exit_critical(key);

        // In os_queue_send nach dem k_sched_request_switch():
        if (current_task->wait_result == OS_ERR_TIMEOUT) {
            //prio_waitq_remove(&qcb->send_waitq, current_task); // <-- Unbedingt austragen!
            port_exit_critical(key);
            return OS_ERR_TIMEOUT; // Oder OS_ERR_TIMEOUT, je nach API-Definition
        }
        //Daten übertragen

        return OS_OK;
    }   
    //3. Wenn Queue platz hat und kein Empfänger  wartet Senden
    ringbuffer_write(&qcb->ring_buf, payload);

    port_exit_critical(key);
    return OS_OK;
}

/*-----------------------------------Empfangen-----------------------*/
os_status_t os_queue_receive(QCB_sctQCB_t *qcb,
                               void *receive_buffer,
                               uint32_t timeout_ticks) {
    
    KERNEL_REQUIRE(qcb != 0);
    KERNEL_REQUIRE(receive_buffer != 0);
    
    uint32_t key = port_enter_critical();

    kernel_task_t *current_task = k_sched_current();
    KERNEL_REQUIRE(current_task != 0);

    //1. Wenn send_waitq nicht leer -> Direkte übergabe
    if(!prio_waitq_is_empty(&qcb->send_waitq) && ringbuffer_is_empty(&qcb->ring_buf)){
        
        kernel_task_t *send_node = prio_waitq_pop_highest(&qcb->send_waitq);
        k_timeout_remove(send_node);
        KERNEL_REQUIRE(send_node != 0);

        send_node->wait_object = 0;
        send_node->wait_type = K_WAIT_NONE;
        send_node->wake_tick = 0;
        send_node->wait_result = OS_OK;
        
        //Direkte Übergabe
        KERNEL_REQUIRE(send_node->wait_data != 0);
        memcpy(receive_buffer, send_node->wait_data, qcb->ring_buf.content_size);
        
        k_sched_task_ready(send_node);
        k_sched_request_switch();

        port_exit_critical(key);

        return OS_OK;
    }

    //2. Wenn send_wait leer und msgq leer
    if(ringbuffer_is_empty(&qcb->ring_buf)){
        // Non Blocking
        if (timeout_ticks == 0) {
            port_exit_critical(key);
            return OS_ERR_EMPTY;
        }

        // Block forever
        else if (timeout_ticks == OS_WAIT_FOREVER) {
            k_sched_task_block(current_task);
            prio_waitq_push(&qcb->receive_waitq, current_task);

            current_task->wait_object = qcb;
            current_task->wait_type = K_WAIT_QUEUE_RECV;
            current_task->wait_data = receive_buffer;

        }

        // Block with Timeout
        else if (timeout_ticks != OS_WAIT_FOREVER && timeout_ticks > 0) {
            k_sched_task_block(current_task);
            prio_waitq_push(&(qcb->receive_waitq), current_task); 

            // Wait Objekt wird übergeben
            current_task->wait_type = K_WAIT_QUEUE_RECV;
            current_task->wait_object = qcb;
            current_task->wait_data = receive_buffer;
            // Timeout Node zuweisung
            k_timeout_add(current_task, timeout_ticks);
        }

        k_sched_request_switch();
        
        port_exit_critical(key);
        
        if(current_task->wait_result == OS_ERR_TIMEOUT){
            //prio_waitq_remove(&qcb->receive_waitq, current_task);
            port_exit_critical(key);
            return OS_ERR_TIMEOUT;
        }

        //Empfang erfolgreich
        return OS_OK;
    }   
    //3. Wenn Queue platz hat und kein Empfänger  wartet Senden (Funktioniert)
    ringbuffer_read(&qcb->ring_buf, receive_buffer);

    port_exit_critical(key); 
    return OS_OK;
}

/*------------------ Timeout Cleanup Funktionen ------------------*/
void k_queue_send_timeout_cleanup(QCB_sctQCB_t *qcb, kernel_task_t *task) {
    
    KERNEL_REQUIRE(qcb != 0);
    KERNEL_REQUIRE(task != 0);

    prio_waitq_remove(&qcb->send_waitq, task);

    task->wait_object = 0;
    task->wait_type = K_WAIT_NONE;
    task->wake_tick = 0;
    task->wait_result = OS_ERR_TIMEOUT;
}

void k_queue_recv_timeout_cleanup(QCB_sctQCB_t *qcb, kernel_task_t *task) {
    KERNEL_REQUIRE(qcb != 0);
    KERNEL_REQUIRE(task != 0);

    prio_waitq_remove(&qcb->receive_waitq, task);


    task->wait_object = 0;
    task->wait_type = K_WAIT_NONE;
    task->wake_tick = 0;
    task->wait_result = OS_ERR_TIMEOUT;
}
