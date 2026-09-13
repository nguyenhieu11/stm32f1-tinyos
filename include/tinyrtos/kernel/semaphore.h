/**
 * @file    semaphore.h
 * @brief   Counting/binary semaphore primitive.
 */

#ifndef _RTOSK_SEMAPHORE_H_
#define _RTOSK_SEMAPHORE_H_

#include <stdint.h>

#define RTOSK_SEMAPHORE_NO_WAITER 0xFFFFFFFFUL

typedef struct {
    uint32_t count;
    uint32_t waiting_task;
} rtosk_semaphore_t;

void rtosk_semaphore_init(rtosk_semaphore_t * sem, uint32_t initial_count);
void rtosk_semaphore_take(rtosk_semaphore_t * sem);
void rtosk_semaphore_give(rtosk_semaphore_t * sem);
void rtosk_semaphore_give_from_isr(rtosk_semaphore_t * sem);

#endif /* _RTOSK_SEMAPHORE_H_ */
