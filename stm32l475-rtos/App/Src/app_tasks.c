#include "app_tasks.h"
#include "kernel_task.h"
#include "os_mutex.h"
#include "os_task.h"
#include "os_delay.h"
#include "os_types.h"
#include "stm32l4xx_hal.h"
#include "os_message_queue.h"

#define WORKER_COUNT        4u
#define ITERATIONS_PER_TASK 10000u


volatile uint32_t test_error_count = 0;
volatile uint32_t shared_counter = 0;
volatile uint32_t done_count = 0;
volatile uint8_t inside_cs = 0u;

//Global für queue_test
volatile uint32_t recv_success_count = 0;
volatile uint32_t recv_timeout_count = 0;
volatile uint32_t send_success_count = 0;
volatile uint32_t send_timeout_count = 0;

uint32_t test_arr[10];
QCB_sctQCB_t test_QCB;



void send_test_task(void){
    uint32_t send_buf = 500;


    while(1){
        
        
        os_status_t result = os_queue_send(&test_QCB, &send_buf, OS_WAIT_FOREVER);
        if(result == OS_ERR_TIMEOUT){
            send_timeout_count++;
        }
        else if(result == OS_OK){
            send_success_count++;
        }
        send_buf = send_buf + 1;

        os_delay(200u);
        
    }
} 

void receive_test_task(void){
    uint32_t recv_buf;

    
    while(1){
       
        os_status_t result = os_queue_receive(&test_QCB, &recv_buf, 0u);
        if(result == OS_ERR_TIMEOUT){
            recv_timeout_count++;
        }
        else if(result == OS_OK){
            recv_success_count++;
        } 
        os_delay(10u);
    }
}
    void queue_test_init(){
    
        os_queue_create(&test_QCB, "TEST", 1, test_arr, sizeof(uint32_t), 10);
    
        os_task_create(send_test_task, 2);
        os_task_create(receive_test_task, 2);
    }