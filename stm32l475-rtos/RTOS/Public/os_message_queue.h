#include <stdint.h>
#include "/home/martin/Dokumente/Programmierung/Echtzeitsystem Git/RTOS/stm32l475-rtos/RTOS/Internal/Inc/tcb.h"
#include "/home/martin/Dokumente/Programmierung/Echtzeitsystem Git/RTOS/stm32l475-rtos/RTOS/Internal/Inc/prio_waitq.h"
#include "/home/martin/Dokumente/Programmierung/Echtzeitsystem Git/RTOS/stm32l475-rtos/Port/CortexM/Inc/port.h"
#include "/home/martin/Dokumente/Programmierung/Echtzeitsystem Git/RTOS/stm32l475-rtos/RTOS/Kernel/Inc/k_sched.h"
#define WAIT_FOREVER 0xFFFFFFFF


typedef struct{
    char *sQName;
    uint8_t u8QID;
    uint8_t u8QLength;
    uint8_t u8MessageLength;

    //Ringspeicher
    uint8_t *pBuffer;
    uint8_t uReadIndex;
    uint8_t uWriteIndex;
    uint8_t uMessageCount;

    prio_waitq_t send_queue;
    prio_waitq_t receive_queue;
} QCB_sctQCB_t;

void Queue_Create(QCB_sctQCB_t *qcb, const char *QName, uint8_t QID, uint8_t QLength, uint8_t MLength, kernel_task_t *currentTask);
void Queue_Send(QCB_sctQCB_t *qcb, void *payload, uint32_t TimeoutTicks, kernel_task_t *currentTask);
void Queue_Receive(QCB_sctQCB_t *qcb, void *ReceivePuffer, uint32_t TimeoutTicks, kernel_task_t *currentTask);
