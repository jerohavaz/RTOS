#include <stdint.h>
#include "tcb.h"
#include "prio_waitq.h"
#include "os_types.h"
#include "ringbuffer.h"

typedef struct {
    char *name;
    uint8_t id;
    ringbuffer_t ring_buf;
    prio_waitq_t send_waitq;
    prio_waitq_t receive_waitq;
} QCB_sctQCB_t;

void os_queue_create(QCB_sctQCB_t *qcb,
                     const char *name,
                     uint8_t id,
                     void *buffer,
                     uint32_t msg_len,
                     uint32_t q_length);
os_status_t os_queue_send(QCB_sctQCB_t *qcb,
                            const void *payload,
                            uint32_t timeout_ticks);
os_status_t os_queue_receive(QCB_sctQCB_t *qcb,
                               void *receive_buffer,
                               uint32_t timeout_ticks);

// Cleanup Prototypen
void k_queue_send_timeout_cleanup(QCB_sctQCB_t *qcb, kernel_task_t *task);
void k_queue_recv_timeout_cleanup(QCB_sctQCB_t *qcb, kernel_task_t *task);
