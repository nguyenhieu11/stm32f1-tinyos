/**
 * @file    task.h
 * @brief   TinyRTOS task management (TCB, task creation, stack management).
 */

#ifndef _RTOSK_TASK_H_
#define _RTOSK_TASK_H_

#include <stdint.h>
#include <tinyrtos/kernel/kernel.h>

/* Stack depth in words (256 × 4 = 1024 bytes per task) */
#define RTOSK_TASK_STACK_WORDS 256UL

/* Maximum user tasks */
#define RTOSK_MAX_TASKS 4UL

/* Index of idle task (one past user tasks) */
#define RTOSK_IDLE_TASK_INDEX RTOSK_MAX_TASKS

/* Total slots including idle */
#define RTOSK_TOTAL_TASK_SLOTS (RTOSK_MAX_TASKS + 1UL)

/* Stack watermark pattern */
#define RTOSK_STACK_WATERMARK 0xDEADBEEFUL

typedef enum {
    RTOSK_TASK_READY = 0U,
    RTOSK_TASK_BLOCKED,
    RTOSK_TASK_BLOCKED_ON_SEMAPHORE,
    RTOSK_TASK_BLOCKED_ON_QUEUE,
    RTOSK_TASK_BLOCKED_ON_MUTEX,
    RTOSK_TASK_IDLE
} rtosk_task_state_t;

struct rtosk_task_info {
    uint32_t index;
    uint32_t priority;
    rtosk_task_state_t state;
    uint32_t wake_tick;
    uint32_t stack_used_words;
    uint32_t stack_free_words;
    const char * name;
};

typedef struct {
    uint32_t * sp;
    uint32_t stack[RTOSK_TASK_STACK_WORDS];
    rtosk_task_state_t state;
    uint32_t wake_tick;
    uint32_t priority;
    const char * name;
} rtosk_task_t;

void rtosk_task_create(rtosk_task_func_t task_func, uint32_t priority, const char * name);
void rtosk_task_init_idle(void);
void rtosk_task_save_stack_pointer(uint32_t * sp);
void rtosk_task_block_current_until(uint32_t wake_tick);
void rtosk_task_block_current_on_semaphore(void);
void rtosk_task_block_current_on_queue(void);
void rtosk_task_block_current_on_mutex(void);
void rtosk_task_set_ready(uint32_t index);
uint32_t rtosk_task_update_blocked(uint32_t current_tick);
uint32_t * rtosk_task_get_stack_pointer(void);
uint32_t rtosk_task_get_count(void);
uint32_t rtosk_task_get_current_index(void);
void rtosk_task_set_current_index(uint32_t index);
rtosk_task_t * rtosk_task_get(uint32_t index);
uint32_t rtosk_task_is_idle_ready(void);
uint32_t rtosk_task_get_info(uint32_t index, rtosk_task_info_t * info);
const char * rtosk_task_state_to_string(rtosk_task_state_t state);

#endif /* _RTOSK_TASK_H_ */
