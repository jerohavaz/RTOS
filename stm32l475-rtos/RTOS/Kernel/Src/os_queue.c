#include "os_queue.h"
#include "k_sched.h"
#include "k_timeout.h"
#include "kernel_panic.h"
#include "os_types.h"
#include "port.h"
#include "prio_waitq.h"
#include <string.h>

static kernel_task_list_node_t *sched_node(kernel_task_t *task) {
    return &task->sched_node;
}


// TODO: AUSLAGERNNNN
static os_status_t queue_check_timeout_arg(uint32_t timeout_ticks) {
    /*
     * timeout_list ordering uses signed tick subtraction, so delays must stay
     * below 2^31 ticks.
     */
    if ((timeout_ticks != OS_WAIT_FOREVER) && (timeout_ticks >= 0x80000000u)) {
        return OS_ERR_INVALID_ARG;
    }

    return OS_OK;
}

os_status_t os_queue_init(os_queue_t *queue, void *storage, uint32_t msg_size, uint32_t msg_count) {
    if ((queue == 0) || (storage == 0)) {
        return OS_ERR_NULL;
    }

    if ((msg_size == 0u) || (msg_count == 0u)) {
        return OS_ERR_INVALID_ARG;
    }

    ring_msgbuf_init(&queue->buffer, storage, msg_size, msg_count);

    prio_waitq_init(&queue->send_wait_list, sched_node);
    prio_waitq_init(&queue->recv_wait_list, sched_node);

    return OS_OK;
}

uint8_t os_queue_is_empty(os_queue_t *queue) {
    if (queue == 0) {
        return 1u;
    }

    uint32_t key = port_enter_critical();
    uint8_t result = ring_msgbuf_is_empty(&queue->buffer);
    port_exit_critical(key);

    return result;
}

uint8_t os_queue_is_full(os_queue_t *queue) {
    if (queue == 0) {
        return 0u;
    }

    uint32_t key = port_enter_critical();
    uint8_t result = ring_msgbuf_is_full(&queue->buffer);
    port_exit_critical(key);

    return result;
}

os_status_t os_queue_send(os_queue_t *queue, const void *msg, uint32_t timeout_ticks) {
    if ((queue == 0) || (msg == 0)) {
        return OS_ERR_NULL;
    }

    os_status_t status = queue_check_timeout_arg(timeout_ticks);

    if (status != OS_OK) {
        return status;
    }

    uint32_t key = port_enter_critical();

    /*
     * Direct handoff:
     * If a receiver is already blocked, copy directly into its output buffer.
     */
    kernel_task_t *recv_task = prio_waitq_pop_highest(&queue->recv_wait_list);

    if (recv_task != 0) {
        KERNEL_REQUIRE(recv_task->wait_type == K_WAIT_QUEUE_RECV);
        KERNEL_REQUIRE(recv_task->wait_object == queue);
        KERNEL_REQUIRE(recv_task->wait_data != 0);

        k_timeout_try_remove(recv_task);

        memcpy(recv_task->wait_data, msg, ring_msgbuf_msg_size(&queue->buffer));

        recv_task->wait_data = 0;
        recv_task->wait_type = K_WAIT_NONE;
        recv_task->wait_object = 0;
        recv_task->wait_result = OS_OK;

        k_sched_task_ready(recv_task);

        port_exit_critical(key);

        k_sched_request_switch();

        return OS_OK;
    }

    /*
     * Normal buffered send.
     */
    if (!ring_msgbuf_is_full(&queue->buffer)) {
        ring_msgbuf_push(&queue->buffer, msg);

        port_exit_critical(key);
        return OS_OK;
    }

    if (timeout_ticks == OS_NO_WAIT) {
        port_exit_critical(key);
        return OS_ERR_WOULD_BLOCK;
    }

    /*
     * From here on, send would block.
     */
    if (port_in_exception() != 0u) {
        port_exit_critical(key);
        return OS_ERR_IN_ISR;
    }

    kernel_task_t *current = k_sched_current();

    if (current == 0) {
        port_exit_critical(key);
        return OS_ERR_INVALID_STATE;
    }

    if (k_sched_is_idle(current) != 0u) {
        port_exit_critical(key);
        return OS_ERR_INVALID_STATE;
    }

    current->wait_object = queue;
    current->wait_type = K_WAIT_QUEUE_SEND;
    current->wait_data = (void *)msg;
    current->wait_result = OS_ERR_BUSY;

    prio_waitq_push(&queue->send_wait_list, current);

    if (timeout_ticks != OS_WAIT_FOREVER) {
        k_timeout_add(current, timeout_ticks);
    }

    k_sched_task_block(current);

    port_exit_critical(key);

    k_sched_request_switch();

    /*
     * With direct handoff:
     * OS_OK means the message was copied into either a receiver buffer or the ring.
     * OS_ERR_TIMEOUT means it was removed from the send wait list.
     */
    return current->wait_result;
}

os_status_t os_queue_recv(os_queue_t *queue, void *msg_out, uint32_t timeout_ticks) {
    if ((queue == 0) || (msg_out == 0)) {
        return OS_ERR_NULL;
    }

    os_status_t status = queue_check_timeout_arg(timeout_ticks);

    if (status != OS_OK) {
        return status;
    }

    uint32_t key = port_enter_critical();

    /*
     * Prefer buffered messages first to preserve FIFO order.
     */
    if (!ring_msgbuf_is_empty(&queue->buffer)) {
        ring_msgbuf_pop(&queue->buffer, msg_out);

        /*
         * A ring slot was freed.
         * If a sender is blocked, complete that sender's send immediately by
         * copying its message into the freed queue slot.
         */
        kernel_task_t *send_task = prio_waitq_pop_highest(&queue->send_wait_list);

        if (send_task != 0) {
            KERNEL_REQUIRE(send_task->wait_type == K_WAIT_QUEUE_SEND);
            KERNEL_REQUIRE(send_task->wait_object == queue);
            KERNEL_REQUIRE(send_task->wait_data != 0);
            KERNEL_REQUIRE(!ring_msgbuf_is_full(&queue->buffer));

            k_timeout_try_remove(send_task);

            ring_msgbuf_push(&queue->buffer, send_task->wait_data);

            send_task->wait_data = 0;
            send_task->wait_type = K_WAIT_NONE;
            send_task->wait_object = 0;
            send_task->wait_result = OS_OK;

            k_sched_task_ready(send_task);
        }

        port_exit_critical(key);

        if (send_task != 0) {
            k_sched_request_switch();
        }

        return OS_OK;
    }

    /*
     * Queue is empty, but a sender may already be blocked.
     * In that case, copy directly from sender's message buffer to receiver.
     */
    kernel_task_t *send_task = prio_waitq_pop_highest(&queue->send_wait_list);

    if (send_task != 0) {
        KERNEL_REQUIRE(send_task->wait_type == K_WAIT_QUEUE_SEND);
        KERNEL_REQUIRE(send_task->wait_object == queue);
        KERNEL_REQUIRE(send_task->wait_data != 0);

        k_timeout_try_remove(send_task);

        memcpy(msg_out, send_task->wait_data, ring_msgbuf_msg_size(&queue->buffer));

        send_task->wait_data = 0;
        send_task->wait_type = K_WAIT_NONE;
        send_task->wait_object = 0;
        send_task->wait_result = OS_OK;

        k_sched_task_ready(send_task);

        port_exit_critical(key);

        k_sched_request_switch();

        return OS_OK;
    }

    if (timeout_ticks == OS_NO_WAIT) {
        port_exit_critical(key);
        return OS_ERR_WOULD_BLOCK;
    }

    /*
     * From here on, recv would block.
     */
    if (port_in_exception() != 0u) {
        port_exit_critical(key);
        return OS_ERR_IN_ISR;
    }

    kernel_task_t *current = k_sched_current();

    if (current == 0) {
        port_exit_critical(key);
        return OS_ERR_INVALID_STATE;
    }

    if (k_sched_is_idle(current) != 0u) {
        port_exit_critical(key);
        return OS_ERR_INVALID_STATE;
    }

    current->wait_object = queue;
    current->wait_type = K_WAIT_QUEUE_RECV;
    current->wait_data = msg_out;
    current->wait_result = OS_ERR_BUSY;

    prio_waitq_push(&queue->recv_wait_list, current);

    if (timeout_ticks != OS_WAIT_FOREVER) {
        k_timeout_add(current, timeout_ticks);
    }

    k_sched_task_block(current);

    port_exit_critical(key);

    k_sched_request_switch();

    /*
     * With direct handoff:
     * OS_OK means msg_out was already filled before this task was readied.
     */
    return current->wait_result;
}

void k_queue_send_timeout_cleanup(os_queue_t *queue, kernel_task_t *task) {
    KERNEL_REQUIRE(queue != 0);
    KERNEL_REQUIRE(task != 0);
    KERNEL_REQUIRE(task->wait_type == K_WAIT_QUEUE_SEND);
    KERNEL_REQUIRE(task->wait_object == queue);

    prio_waitq_remove(&queue->send_wait_list, task);

    task->wait_data = 0;
    task->wait_type = K_WAIT_NONE;
    task->wait_object = 0;
    task->wait_result = OS_ERR_TIMEOUT;
}

void k_queue_recv_timeout_cleanup(os_queue_t *queue, kernel_task_t *task) {
    KERNEL_REQUIRE(queue != 0);
    KERNEL_REQUIRE(task != 0);
    KERNEL_REQUIRE(task->wait_type == K_WAIT_QUEUE_RECV);
    KERNEL_REQUIRE(task->wait_object == queue);

    prio_waitq_remove(&queue->recv_wait_list, task);

    task->wait_data = 0;
    task->wait_type = K_WAIT_NONE;
    task->wait_object = 0;
    task->wait_result = OS_ERR_TIMEOUT;
}


