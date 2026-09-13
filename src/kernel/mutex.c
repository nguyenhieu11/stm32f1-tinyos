/**
 * @file    mutex.c
 * @brief   Mutex implementation (no priority inheritance).
 */

#include <tinyrtos/kernel/mutex.h>
#include <tinyrtos/kernel/kernel.h>
#include <tinyrtos/kernel/task.h>

void rtosk_mutex_init(rtosk_mutex_t * mutex) {
    if(mutex == 0) return;
    mutex->owner = RTOSK_MUTEX_NO_OWNER;
    mutex->waiting_task = RTOSK_MUTEX_NO_WAITER;
}

void rtosk_mutex_lock(rtosk_mutex_t * mutex) {
    if(mutex == 0) return;
    for(;;) {
        rtosk_kernel_enter_critical();
        uint32_t current = rtosk_task_get_current_index();
        if(mutex->owner == RTOSK_MUTEX_NO_OWNER) {
            mutex->owner = current;
            rtosk_kernel_exit_critical();
            return;
        }
        if(mutex->owner == current) {
            rtosk_kernel_exit_critical();
            return;
        }
        mutex->waiting_task = current;
        rtosk_task_block_current_on_mutex();
        rtosk_kernel_exit_critical();
        rtosk_kernel_yield();
    }
}

void rtosk_mutex_unlock(rtosk_mutex_t * mutex) {
    if(mutex == 0) return;
    rtosk_kernel_enter_critical();
    uint32_t current = rtosk_task_get_current_index();
    if(mutex->owner != current) {
        rtosk_kernel_exit_critical();
        return;
    }
    mutex->owner = RTOSK_MUTEX_NO_OWNER;
    if(mutex->waiting_task != RTOSK_MUTEX_NO_WAITER) {
        rtosk_task_set_ready(mutex->waiting_task);
        mutex->waiting_task = RTOSK_MUTEX_NO_WAITER;
    }
    rtosk_kernel_exit_critical();
    rtosk_kernel_yield();
}
