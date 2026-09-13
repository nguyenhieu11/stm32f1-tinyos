/**
 * @file    led.c
 * @brief   Blue Pill LED driver for PC13 (onboard) and PA0 (external).
 */

#include "led.h"
#include "stm32f103x8.h"

void bsp_led_init(void) {
    /* Enable GPIOC and GPIOA clocks */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    /* PC13: push-pull output, 2 MHz (onboard LED, active low) */
    GPIOC->CRH &= ~(0xFUL << ((LED13_PIN - 8) * 4));
    GPIOC->CRH |=  (GPIO_OUT_PP_2MHZ << ((LED13_PIN - 8) * 4));

    /* PA0: push-pull output, 2 MHz (external LED) */
    GPIOA->CRL &= ~(0xFUL << (LED0_PIN * 4));
    GPIOA->CRL |=  (GPIO_OUT_PP_2MHZ << (LED0_PIN * 4));

    /* Start with LEDs off */
    bsp_led13_off();
    bsp_led0_off();
}

void bsp_led13_on(void) {
    /* PC13 active low: BRR clears bit → pin low → LED on */
    GPIOC->BRR = (1UL << LED13_PIN);
}

void bsp_led13_off(void) {
    /* BSRR sets bit → pin high → LED off */
    GPIOC->BSRR = (1UL << LED13_PIN);
}

void bsp_led13_toggle(void) {
    GPIOC->ODR ^= (1UL << LED13_PIN);
}

void bsp_led0_on(void) {
    /* PA0 active high: BSRR sets bit → pin high → LED on */
    GPIOA->BSRR = (1UL << LED0_PIN);
}

void bsp_led0_off(void) {
    GPIOA->BRR = (1UL << LED0_PIN);
}

void bsp_led0_toggle(void) {
    GPIOA->ODR ^= (1UL << LED0_PIN);
}
