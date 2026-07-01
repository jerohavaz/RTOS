#include <stdlib.h>
#include <stdint.h>
#include <string.h>


typedef struct{
    uint8_t *p_Buffer;
    uint32_t length;
    uint32_t content_size;
    uint8_t read_index;
    uint8_t write_index;
    uint8_t content_count;
    
}ringbuffer_t;

void ringbuffer_init(ringbuffer_t *rb, void *storage, uint32_t elem_size, uint32_t elem_count);
void ringbuffer_write(ringbuffer_t *rb, const void *msg);
void ringbuffer_read(ringbuffer_t *rb, void *data_sink);

uint8_t ringbuffer_is_empty(const ringbuffer_t *rb);
uint8_t ringbuffer_is_full(const ringbuffer_t *rb);

uint32_t ringbuffer_count(const ringbuffer_t *rb);
uint32_t ringbuffer_length(const ringbuffer_t *rb);
uint32_t ringbuffer_msg_size(const ringbuffer_t *rb);
