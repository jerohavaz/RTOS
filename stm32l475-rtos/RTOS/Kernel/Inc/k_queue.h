#ifndef K_QUEUE_H_
#define K_QUEUE_H_

#include "kernel_task.h"
#include "os_queue.h"

void k_queue_send_timeout_cleanup(os_queue_t *queue, kernel_task_t *task);
void k_queue_recv_timeout_cleanup(os_queue_t *queue, kernel_task_t *task);

#endif