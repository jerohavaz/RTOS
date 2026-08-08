#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "app_tasks.h"
#include "os_task.h"
#include "os_delay.h"
#include "os_queue.h"
#include "os_types.h"
#include "shell.h"
#include "stm32l4xx_hal.h"
#include "stm32l475e_iot01_accelero.h"
#include "stm32l475e_iot01_gyro.h"
#include "stm32l4xx_hal_def.h"
#include "stm32l4xx_hal_uart.h"
#include "trace.h"



#define QUEUE_MSG_SIZE              (sizeof(char) * QUEUE_MSG_SYMBOL_COUNT)
#define QUEUE_MSG_COUNT             8u
#define QUEUE_MSG_SYMBOL_COUNT      60u


static int16_t acc[3];
static float gyro[3];

static os_queue_t uart_queue;
static char queue_storage[QUEUE_MSG_COUNT * QUEUE_MSG_SIZE];
extern UART_HandleTypeDef huart1;

volatile uint32_t test_error_count = 0u;

static void test_fail(void) {
    test_error_count++;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
}

static void format_milli(char *out, size_t out_size, int32_t milli) {
  char sign = '\0';

  if (milli < 0) {
    sign = '-';
    milli = -milli;
  }

  int32_t whole = milli / 1000;
  int32_t frac = milli % 1000;

  if (sign) {
    snprintf(out, out_size, "-%ld.%03ld", (long)whole, (long)frac);
  } else {
    snprintf(out, out_size, "%ld.%03ld", (long)whole, (long)frac);
  }
}

static void sensor_task(void){
    os_status_t status;
    char msg_buf[QUEUE_MSG_SYMBOL_COUNT] = {0};


    if(BSP_ACCELERO_Init() != ACCELERO_OK){
        snprintf(msg_buf, sizeof(msg_buf), "ACCEL init failed\r\n");
        status = os_queue_send(&uart_queue, msg_buf, OS_WAIT_FOREVER); //Important info -> needs to get into the Queue
        if(status != OS_OK) test_fail();
    }

    if(BSP_GYRO_Init() != GYRO_OK){
        snprintf(msg_buf, sizeof(msg_buf), "GYRO init failed\r\n");
        status = os_queue_send(&uart_queue, msg_buf, OS_WAIT_FOREVER); //Important info -> needs to get into the Queue
        if(status != OS_OK) test_fail();
    }

    snprintf(msg_buf, sizeof(msg_buf), "ax_g ; ay_g ; az_g | gx_dps ; gy_ps ; gz_ps\r\n");
    status = os_queue_send(&uart_queue, msg_buf, OS_WAIT_FOREVER); //Important info -> needs to get into the Queue (Could be removed)
    if(status != OS_OK) test_fail();
    
    
    while(1){

        trace_sensor_read();

        BSP_ACCELERO_AccGetXYZ(acc);
        BSP_GYRO_GetXYZ(gyro);

        char ax[16], ay[16], az[16];
        char gx[16], gy[16], gz[16];

        format_milli(ax, sizeof(ax), acc[0]);
        format_milli(ay, sizeof(ay), acc[1]);
        format_milli(az, sizeof(az), acc[2]);

        format_milli(gx, sizeof(gx), (int32_t)gyro[0]);
        format_milli(gy, sizeof(gy), (int32_t)gyro[1]);
        format_milli(gz, sizeof(gz), (int32_t)gyro[2]);

        int len = snprintf(msg_buf, sizeof(msg_buf), "%s ; %s ; %s | %s ; %s ; %s\r\n", ax, ay, az, gx, gy, gz);
        if(len > 0 && len < (int) sizeof(msg_buf)){
            status = os_queue_send(&uart_queue, msg_buf, 10);
        }
        os_delay(30); // setting for how fast the values are transmitted
    }
}

static void uart_task(void){

    os_status_t status;
    char recv_buf[QUEUE_MSG_SYMBOL_COUNT] = {0};
    shell_init(&huart1);

    while(1){
        shell_update();

        status = os_queue_recv(&uart_queue, recv_buf, 10);
        if(status != OS_OK){ 
          test_fail();
        }
        else{
            if(is_stream_enabled()){
            HAL_UART_Transmit(&huart1, (uint8_t *) recv_buf, (uint16_t) strlen(recv_buf), HAL_MAX_DELAY);
            trace_transmission_complete();
            }
        }

    }
}

static void expect_status(os_status_t actual, os_status_t expected) {
    if (actual != expected) {
        test_fail();
    }
}


void app_tasks_init(void) {
    expect_status(os_queue_init(&uart_queue, queue_storage, QUEUE_MSG_SIZE, 6), OS_OK);
    expect_status(os_task_create(sensor_task, 4), OS_OK);
    expect_status(os_task_create(uart_task, 4), OS_OK);
}