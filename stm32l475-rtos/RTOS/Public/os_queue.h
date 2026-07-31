#ifndef OS_QUEUE_H_
#define OS_QUEUE_H_

#include "kernel_task.h"
#include "os_types.h"
#include "prio_waitq.h"
#include "ring_msgbuf.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    ring_msgbuf_t buffer;
    prio_waitq_t send_wait_list;
    prio_waitq_t recv_wait_list;
} os_queue_t;

os_status_t os_queue_init(os_queue_t *queue, void *storage, uint32_t msg_size, uint32_t msg_count);

os_status_t os_queue_send(os_queue_t *queue, const void *msg, uint32_t timeout_ticks);

os_status_t os_queue_recv(os_queue_t *queue, void *msg_out, uint32_t timeout_ticks);

bool os_queue_is_empty(os_queue_t *queue);
bool os_queue_is_full(os_queue_t *queue);

#endif