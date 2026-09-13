/**
 * @file    semaphore.c
 * @brief   Counting/binary semaphore implementation.
 */

#include <tinyrtos/kernel/semaphore.h>
#include <tinyrtos/kernel/kernel.h>
#include <tinyrtos/kernel/task.h>
#include <tinyrtos/kernel/port.h>

void rtosk_semaphore_init(rtosk_semaphore_t * sem, uint32_t initial_count) {
    if(sem == 0) return;
    sem->count = initial_count;
    sem->waiting_task = RTOSK_SEMAPHORE_NO_WAITER;
}

void rtosk_semaphore_take(rtosk_semaphore_t * sem) {
    if(sem == 0) return;
    rtosk_kernel_enter_critical();
    if(sem->count > 0UL) {
        sem->count--;
        rtosk_kernel_exit_critical();
        return;
    }
    sem->waiting_task = rtosk_task_get_current_index();
    rtosk_task_block_current_on_semaphore();
    rtosk_kernel_exit_critical();
    rtosk_kernel_yield();
}

void rtosk_semaphore_give(rtosk_semaphore_t * sem) {
    if(sem == 0) return;
    rtosk_kernel_enter_critical();
    if(sem->waiting_task != RTOSK_SEMAPHORE_NO_WAITER) {
        rtosk_task_set_ready(sem->waiting_task);
        sem->waiting_task = RTOSK_SEMAPHORE_NO_WAITER;
    } else {
        sem->count = 1UL;
    }
    rtosk_kernel_exit_critical();
    rtosk_kernel_yield();
}

void rtosk_semaphore_give_from_isr(rtosk_semaphore_t * sem) {
    if(sem == 0) return;
    if(sem->waiting_task != RTOSK_SEMAPHORE_NO_WAITER) {
        rtosk_task_set_ready(sem->waiting_task);
        sem->waiting_task = RTOSK_SEMAPHORE_NO_WAITER;
        rtosk_port_yield();
    } else {
        sem->count = 1UL;
    }
}
