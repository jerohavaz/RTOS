#ifndef RING_MSGBUF_H_
#define RING_MSGBUF_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t *buffer;
    uint32_t msg_size;
    uint32_t capacity;
    uint32_t count;
    uint32_t head;
    uint32_t tail;
} ring_msgbuf_t;

void ring_msgbuf_init(ring_msgbuf_t *rb, void *buffer, uint32_t msg_size, uint32_t capacity);

bool ring_msgbuf_is_empty(const ring_msgbuf_t *rb);
bool ring_msgbuf_is_full(const ring_msgbuf_t *rb);

uint32_t ring_msgbuf_count(const ring_msgbuf_t *rb);
uint32_t ring_msgbuf_capacity(const ring_msgbuf_t *rb);
uint32_t ring_msgbuf_msg_size(const ring_msgbuf_t *rb);

void ring_msgbuf_push(ring_msgbuf_t *rb, const void *msg);
void ring_msgbuf_pop(ring_msgbuf_t *rb, void *msg_out);

#endif