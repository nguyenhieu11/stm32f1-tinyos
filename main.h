#ifndef _MAIN_H
#define _MAIN_H

#include <stdint.h>
#include "stm32f103x8.h"

/*
 * Pin assignments for the Blue Pill (STM32F103C8T6):
 *   PC13: Onboard LED (active low — set pin low to turn on)
 *   PB1:  Button (active low — connects to ground when pressed)
 */
#define LED_PIN       (13)
#define LED_PORT      GPIOC
#define BUTTON_PIN    (1)
#define BUTTON_PORT   GPIOB

#endif /* _MAIN_H */
