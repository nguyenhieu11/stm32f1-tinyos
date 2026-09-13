/**
 * @file    main.c
 * @brief   TinyRTOS demo: two preemptive LED blink tasks on STM32F103C8T6.
 *
 * Task 1 (priority 2): Toggle PC13 (onboard LED) every 200ms.
 * Task 2 (priority 1): Toggle PA0  (external LED) every 1000ms.
 *
 * This proves preemptive scheduling: even if one task sleeps,
 * the other continues to run at its own rate.
 */

#include "main.h"
#include "clock.h"
#include "led.h"

#include <tinyrtos/kernel/kernel.h>

/* Task 1: Fast blink — PC13, every 200ms, priority 2 (higher) */
static void fast_blink_task(void) {
    for(;;) {
        bsp_led13_toggle();
        rtosk_kernel_sleep_ms(200);
    }
}

/* Task 2: Slow blink — PA0, every 1000ms, priority 1 (lower) */
static void slow_blink_task(void) {
    for(;;) {
        bsp_led0_toggle();
        rtosk_kernel_sleep_ms(1000);
    }
}

int main(void) {
    /* 1. Configure system clock: 72 MHz (HSE + PLL) */
    clock_init();

    /* 2. Initialize LED GPIOs */
    bsp_led_init();

    /* 3. Create RTOS tasks (before kernel start) */
    rtosk_kernel_create_task(fast_blink_task, 2, "fast");
    rtosk_kernel_create_task(slow_blink_task, 1, "slow");

    /* 4. Initialize SysTick (1ms tick) */
    rtosk_kernel_systick_init();

    /* 5. Start the RTOS — never returns */
    rtosk_kernel_start();

    /* Should never reach here */
    for(;;) {}
}
