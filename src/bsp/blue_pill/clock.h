/**
 * @file    clock.h
 * @brief   System clock configuration for Blue Pill (72 MHz).
 */

#ifndef _BSP_CLOCK_H_
#define _BSP_CLOCK_H_

/**
 * Configures HSE (8 MHz) + PLL (×9) = 72 MHz.
 * Must be called before SysTick init or any peripheral that depends on clock frequency.
 */
void clock_init(void);

#endif /* _BSP_CLOCK_H_ */
