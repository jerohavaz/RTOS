/**
 * @file ring_msgbuf.c
 * @brief Fixed-size-message ring-buffer implementation.
 * @author Jerome
 * @author Martin
 */

#include "ring_msgbuf.h"
#include "kernel_panic.h"

#include <stdbool.h>
#include <string.h>

void ring_msgbuf_init(ring_msgbuf_t *rb, void *buffer, uint32_t msg_size, uint32_t capacity) {
    KERNEL_REQUIRE(rb != 0);
    KERNEL_REQUIRE(buffer != 0);
    KERNEL_REQUIRE(msg_size != 0u);
    KERNEL_REQUIRE(capacity != 0u);

    rb->buffer = (uint8_t *)buffer;
    rb->msg_size = msg_size;
    rb->capacity = capacity;
    rb->count = 0u;
    rb->head = 0u;
    rb->tail = 0u;
}

bool ring_msgbuf_is_empty(const ring_msgbuf_t *rb) {
    KERNEL_REQUIRE(rb != 0);

    return rb->count == 0u;
}

bool ring_msgbuf_is_full(const ring_msgbuf_t *rb) {
    KERNEL_REQUIRE(rb != 0);

    return rb->count >= rb->capacity;
}

uint32_t ring_msgbuf_count(const ring_msgbuf_t *rb) {
    KERNEL_REQUIRE(rb != 0);

    return rb->count;
}

uint32_t ring_msgbuf_capacity(const ring_msgbuf_t *rb) {
    KERNEL_REQUIRE(rb != 0);

    return rb->capacity;
}

uint32_t ring_msgbuf_msg_size(const ring_msgbuf_t *rb) {
    KERNEL_REQUIRE(rb != 0);

    return rb->msg_size;
}

void ring_msgbuf_push(ring_msgbuf_t *rb, const void *msg) {
    KERNEL_REQUIRE(rb != 0);
    KERNEL_REQUIRE(msg != 0);
    KERNEL_REQUIRE(rb->buffer != 0);
    KERNEL_REQUIRE(rb->msg_size != 0u);
    KERNEL_REQUIRE(rb->capacity != 0u);
    KERNEL_REQUIRE(rb->count < rb->capacity);
    KERNEL_REQUIRE(rb->tail < rb->capacity);

    memcpy(&rb->buffer[rb->tail * rb->msg_size], msg, rb->msg_size);

    rb->tail++;

    if (rb->tail >= rb->capacity) {
        rb->tail = 0u;
    }

    rb->count++;
}

void ring_msgbuf_pop(ring_msgbuf_t *rb, void *msg_out) {
    KERNEL_REQUIRE(rb != 0);
    KERNEL_REQUIRE(msg_out != 0);
    KERNEL_REQUIRE(rb->buffer != 0);
    KERNEL_REQUIRE(rb->msg_size != 0u);
    KERNEL_REQUIRE(rb->capacity != 0u);
    KERNEL_REQUIRE(rb->count != 0u);
    KERNEL_REQUIRE(rb->head < rb->capacity);

    memcpy(msg_out, &rb->buffer[rb->head * rb->msg_size], rb->msg_size);

    rb->head++;

    if (rb->head >= rb->capacity) {
        rb->head = 0u;
    }

    rb->count--;
}