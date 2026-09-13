/**
 * @file    mutex.h
 * @brief   Mutex primitive (no priority inheritance).
 */

#ifndef _RTOSK_MUTEX_H_
#define _RTOSK_MUTEX_H_

#include <stdint.h>

#define RTOSK_MUTEX_NO_OWNER  0xFFFFFFFFUL
#define RTOSK_MUTEX_NO_WAITER 0xFFFFFFFFUL

typedef struct {
    uint32_t owner;
    uint32_t waiting_task;
} rtosk_mutex_t;

void rtosk_mutex_init(rtosk_mutex_t * mutex);
void rtosk_mutex_lock(rtosk_mutex_t * mutex);
void rtosk_mutex_unlock(rtosk_mutex_t * mutex);

#endif /* _RTOSK_MUTEX_H_ */
