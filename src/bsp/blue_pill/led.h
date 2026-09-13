/**
 * @file    led.h
 * @brief   Blue Pill LED driver.
 *
 * PC13 = onboard LED (active low)
 * PA0  = external LED (active high, connect LED + resistor to PA0)
 */

#ifndef _BSP_LED_H_
#define _BSP_LED_H_

#include <stdint.h>

#define LED13_PIN  13   /* PC13 */
#define LED0_PIN   0    /* PA0  */

void bsp_led_init(void);

void bsp_led13_on(void);
void bsp_led13_off(void);
void bsp_led13_toggle(void);

void bsp_led0_on(void);
void bsp_led0_off(void);
void bsp_led0_toggle(void);

#endif /* _BSP_LED_H_ */
