/**
 * @file    scheduler.c
 * @brief   Priority-based preemptive scheduler with round-robin among equal priorities.
 */

#include <tinyrtos/kernel/scheduler.h>
#include <tinyrtos/kernel/task.h>
#include <tinyrtos/kernel/port.h>

void rtosk_scheduler_select_next(void) {
    uint32_t task_count = rtosk_task_get_count();
    if(task_count == 0UL) return;

    uint32_t current = rtosk_task_get_current_index();
    if(current >= task_count) {
        current = 0UL;
    }

    uint32_t best_index = RTOSK_IDLE_TASK_INDEX;
    uint32_t best_priority = 0UL;
    uint32_t found_ready = 0UL;

    /* Scan from current task for round-robin fairness */
    for(uint32_t offset = 0UL; offset < task_count; offset++) {
        uint32_t index = current + offset;
        if(index >= task_count) {
            index -= task_count;
        }
        rtosk_task_t * task = rtosk_task_get(index);
        if(task == 0 || task->state != RTOSK_TASK_READY) continue;
        if(found_ready == 0UL || task->priority > best_priority) {
            best_index = index;
            best_priority = task->priority;
            found_ready = 1UL;
        }
    }

    if(found_ready != 0UL) {
        rtosk_task_set_current_index(best_index);
    } else if(rtosk_task_is_idle_ready() != 0UL) {
        rtosk_task_set_current_index(RTOSK_IDLE_TASK_INDEX);
    }
}
