/**
 * @file    main.c
 * @brief   Minimal TinyRTOS demo: two tasks blinking one LED.
 *
 * This demonstrates the kernel core:
 *   - Task creation and preemptive scheduling
 *   - Cooperative sleep (sleep_ms)
 *   - Context switching via PendSV
 *
 * Hardware: PC13 (Blue Pill onboard LED, active low)
 * Clock: 8 MHz HSI (default, no PLL)
 *
 * Two tasks share the same LED:
 *   Task 1 (priority 2, "fast"): Toggle every 200ms
 *   Task 2 (priority 1, "slow"): Toggle every 1000ms
 *
 * Since Task 1 has higher priority, it preempts Task 2.
 * The LED appears to blink fast most of the time, with
 * occasional longer pauses when Task 2 runs.
 */

#include "main.h"
#include <tinyrtos/kernel/kernel.h>

/**
 * SystemCoreClock — CPU frequency in Hz.
 *
 * This variable is declared extern in stm32f103x8.h and used by
 * the kernel to compute the SysTick reload value:
 *   LOAD = SystemCoreClock / 1000 - 1
 *
 * At 8 MHz HSI (default, no clock configuration):
 *   LOAD = 8000000 / 1000 - 1 = 7999
 *
 * If you later configure HSE + PLL for 72 MHz, change this to
 * 72000000 (or better, set it in a clock_init() function).
 */
uint32_t SystemCoreClock = 8000000UL;

/**
 * Initialize PC13 as push-pull output (onboard LED).
 *
 * The Blue Pill's onboard LED is connected to PC13 via an
 * NPN transistor (active low):
 *   ODR bit 13 = 0 → pin low  → LED ON
 *   ODR bit 13 = 1 → pin high → LED OFF
 */
static void led_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;   /* Enable GPIOC clock */
    LED_PORT->CRH &= ~(0xF << ((LED_PIN - 8) * 4));
    LED_PORT->CRH |=  (GPIO_OUT_PP_2MHZ << ((LED_PIN - 8) * 4));
    LED_PORT->BRR = (1 << LED_PIN);        /* LED on initially */
}

/**
 * Toggle PC13 LED.
 *
 * XOR the ODR bit: if it was 0 (LED on), becomes 1 (LED off), and vice versa.
 */
static void led_toggle(void) {
    LED_PORT->ODR ^= (1 << LED_PIN);
}

/* ================================================================== */
/*  RTOS tasks                                                          */
/* ================================================================== */

/**
 * Fast blink task — toggles LED every 200ms (priority 2, higher).
 *
 * This task has higher priority than slow_task, so the scheduler
 * always picks it first when it becomes READY. The result:
 * the LED blinks rapidly at ~2.5 Hz (200ms on + 200ms off).
 */
static void fast_blink_task(void) {
    for (;;) {
        led_toggle();
        rtosk_kernel_sleep_ms(200);
    }
}

/**
 * Slow blink task — toggles LED every 1000ms (priority 1, lower).
 *
 * This task only runs when fast_task is sleeping. When it does run,
 * it toggles the LED once and goes back to sleep. Because fast_task
 * preempts it immediately when fast_task wakes, the effect of this
 * task is barely visible (it competes with fast_task for LED control).
 *
 * To see both tasks clearly, use two separate LEDs.
 */
static void slow_blink_task(void) {
    for (;;) {
        led_toggle();
        rtosk_kernel_sleep_ms(1000);
    }
}

/* ================================================================== */
/*  Main entry point                                                    */
/* ================================================================== */

int main(void) {
    /*
     * Step 1: Initialize hardware.
     *
     * Set up the LED GPIO. Clock is already at 8 MHz HSI (default).
     * No PLL configuration needed for this demo.
     */
    led_init();

    /*
     * Step 2: Create RTOS tasks (BEFORE kernel start).
     *
     * Tasks are created here, but they don't run yet. The kernel
     * just builds their TCBs and initial stack frames.
     *
     * Priority 2 > 1, so fast_task is always preferred by the scheduler.
     */
    rtosk_kernel_create_task(fast_blink_task, 2, "fast");
    rtosk_kernel_create_task(slow_blink_task, 1, "slow");

    /*
     * Step 3: Initialize SysTick timer (1 ms tick).
     *
     * This configures the SysTick hardware timer and sets
     * PendSV/SysTick interrupt priorities. Must be called
     * after SystemCoreClock is set and before kernel_start.
     */
    rtosk_kernel_systick_init();

    /*
     * Step 4: Start the RTOS (never returns).
     *
     * This triggers SVC #0, which launches fast_blink_task
     * (the highest priority READY task). main() is abandoned.
     */
    rtosk_kernel_start();

    /* Should never reach here */
    for (;;) {}
}
