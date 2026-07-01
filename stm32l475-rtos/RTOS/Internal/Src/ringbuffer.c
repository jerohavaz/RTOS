#include "ringbuffer.h"
#include <stdint.h>
#include <string.h>


void ringbuffer_init(ringbuffer_t *rb, void *storage, uint32_t elem_size, uint32_t elem_count){
    /*KERNEL_REQUIRE FUnktion für:
    rb,
    storage
    elem_size
    elem_count*/
    
    rb->p_Buffer = (uint8_t*) storage;

    rb->length = elem_count;
    rb->content_size = elem_size;

    rb->read_index = 0;
    rb->write_index = 0;
    rb->content_count = 0;
}

void ringbuffer_write(ringbuffer_t *rb, const void *msg){

    //Exakte Byte-Adrsse
    memcpy((void*) &rb->p_Buffer[rb->content_size * rb->write_index], msg, rb->content_size);

    rb->write_index = (rb->write_index + 1) % rb->length;
    rb->content_count++;
}

void ringbuffer_read(ringbuffer_t *rb, void *data_sink){


    memcpy(data_sink, (void*) &rb->p_Buffer[rb->content_size * rb->read_index], rb->content_size);
    memset((void*) &rb->p_Buffer[rb->content_size * rb->read_index], 0, rb->content_size);
    
    rb->read_index = (rb->read_index + 1) % rb->length;
    rb->content_count--;
}

uint8_t ringbuffer_is_empty(const ringbuffer_t *rb){
    return rb->content_count == 0u;
}

uint8_t ringbuffer_is_full(const ringbuffer_t *rb){
    return (rb->content_count >= rb->length);
}

uint32_t ringbuffer_count(const ringbuffer_t *rb){
    return rb->content_count;
}

uint32_t ringbuffer_length(const ringbuffer_t *rb){
    return rb->length;
}

uint32_t ringbuffer_msg_size(const ringbuffer_t *rb){
    return rb->content_size;
}