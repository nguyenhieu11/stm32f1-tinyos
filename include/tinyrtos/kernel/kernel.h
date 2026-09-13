/**
 * @file    kernel.h
 * @brief   TinyRTOS kernel API for STM32F103C8T6 (Cortex-M3).
 *
 * Ported from Ian Wilkey's tinyrtos (Cortex-M7) to Cortex-M3 bare-metal.
 * Original: https://github.com/Ian-Wilkey/tinyrtos — MIT License
 */

#ifndef _RTOSK_KERNEL_H_
#define _RTOSK_KERNEL_H_

#include <stdint.h>

#define RTOSK_KERNEL_VERSION "0.1.0"

/**
 * Function handle for a RTOSK task.
 */
typedef void (*rtosk_task_func_t)(void);

/**
 * Forward declaration of struct in task.h
 */
typedef struct rtosk_task_info rtosk_task_info_t;

void rtosk_kernel_systick_init(void);
void rtosk_kernel_delay_ms(uint32_t ms);
void rtosk_kernel_sleep_ms(uint32_t ms);
uint32_t rtosk_kernel_get_ticks(void);
uint32_t rtosk_get_cpu_freq(void);
void rtosk_kernel_create_task(rtosk_task_func_t task_func, uint32_t priority, const char * name);
uint32_t rtosk_kernel_get_task_info(uint32_t index, rtosk_task_info_t * info);
void rtosk_kernel_yield(void);
void rtosk_kernel_enter_critical(void);
void rtosk_kernel_exit_critical(void);
void rtosk_kernel_start(void);

#endif /* _RTOSK_KERNEL_H_ */
