/**
 * @file    queue.h
 * @brief   Fixed-size ring buffer message queue.
 */

#ifndef _RTOSK_QUEUE_H_
#define _RTOSK_QUEUE_H_

#include <stdint.h>

#define RTOSK_QUEUE_NO_WAITER 0xFFFFFFFFUL

typedef struct {
    uint8_t * buffer;
    uint32_t item_size;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t waiting_task;
} rtosk_queue_t;

void rtosk_queue_init(rtosk_queue_t * queue, void * buffer, uint32_t item_size, uint32_t capacity);
uint32_t rtosk_queue_send(rtosk_queue_t * queue, const void * item);
uint32_t rtosk_queue_send_from_isr(rtosk_queue_t * queue, const void * item);
uint32_t rtosk_queue_receive(rtosk_queue_t * queue, void * item);

#endif /* _RTOSK_QUEUE_H_ */
