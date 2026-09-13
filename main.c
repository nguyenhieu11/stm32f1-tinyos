/**
 * Main program for STM32F103C8T6 — Part 4: Hardware Interrupts (EXTI).
 *
 * Based on Vivonomicon's "Bare Metal STM32 Programming Part 4"
 * (https://vivonomicon.com/2018/04/28/bare-metal-stm32-programming-part-4-intro-to-hardware-interrupts/)
 * adapted for the STM32F103C8T6 (Blue Pill).
 *
 * Key STM32F0 → STM32F1 differences:
 *   - STM32F0 uses SYSCFG->EXTICR for EXTI line mapping
 *   - STM32F1 uses AFIO->EXTICR for EXTI line mapping
 *   - STM32F0 groups EXTI lines (EXTI0_1_IRQn)
 *   - STM32F1 has individual EXTI IRQs (EXTI1_IRQn for line 1)
 *   - STM32F1 GPIO uses CRL/CRH (not MODER/PUPDR)
 *
 * Hardware:
 *   - PC13: Onboard LED (active low — set pin low to turn on)
 *   - PB1:  Button input (active low — connects to GND when pressed)
 *
 * Behavior:
 *   - Button press (falling edge) toggles 'led_blink' flag via EXTI interrupt
 *   - led_blink = 1: LED blinks (default)
 *   - led_blink = 0: LED stays on solid
 *
 * Open source under the MIT License
 */

#include "main.h"

/* Global variable shared between main loop and interrupt handler.
 * led_blink = 1: LED blinks (default)
 * led_blink = 0: LED stays on solid (after button press) */
volatile uint8_t led_blink = 1;

/**
 * EXTI1 interrupt handler.
 *
 * Called when a falling edge is detected on EXTI line 1 (PB1).
 * The handler name must match the vector table entry exactly:
 * 'EXTI1_handler' (defined in vector_table.S, weak-aliased to
 * default_interrupt_handler).
 *
 * On STM32F1, EXTI lines 0-4 each have their own IRQ handler.
 * (On STM32F0, lines 0-1 share EXTI0_1_IRQ_handler.)
 */
void EXTI1_handler(void) {
    /* Check that EXTI line 1 is the one that triggered. */
    if (EXTI->PR & (1 << BUTTON_PIN)) {
        /* Clear the interrupt flag by writing 1 to the pending bit. */
        EXTI->PR |= (1 << BUTTON_PIN);

        /* Toggle the blink mode. */
        led_blink = !led_blink;
    }
}

int main(void) {
    /*
     * Step 1: Enable peripheral clocks.
     *
     * On STM32F1, AFIO clock must be enabled before configuring
     * EXTI line mapping. GPIO clocks are on APB2.
     */
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;   /* AFIO clock (for EXTI) */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;   /* GPIOC clock (LED) */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;   /* GPIOB clock (button) */

    /*
     * Step 2: Configure PB1 as input with pull-up (button).
     *
     * PB1 is in CRL (pins 0-7), bits [7:4] (pin 1 → offset 1*4=4).
     * Configuration: CNF=10 (input with pull-up/pull-down), MODE=00 (input).
     *   → 4-bit value = 0x8 (CNF=10, MODE=00)
     *
     * Then set ODR bit 1 = 1 to select pull-up (ODR=0 would be pull-down).
     */
    BUTTON_PORT->CRL &= ~(0xF << (BUTTON_PIN * 4));
    BUTTON_PORT->CRL |=  (GPIO_IN_PULL << (BUTTON_PIN * 4));
    BUTTON_PORT->ODR |=  (1 << BUTTON_PIN);  /* Pull-up */

    /*
     * Step 3: Configure PC13 as push-pull output (LED).
     *
     * PC13 is in CRH (pin 8-15), bits [23:20] (pin 13 → offset 13-8=5, 5*4=20).
     * Configuration: CNF=00 (push-pull output), MODE=10 (2 MHz).
     *   → 4-bit value = 0x2 (CNF=00, MODE=10)
     */
    LED_PORT->CRH &= ~(0xF << ((LED_PIN - 8) * 4));
    LED_PORT->CRH |=  (GPIO_OUT_PP_2MHZ << ((LED_PIN - 8) * 4));

    /*
     * Step 4: Turn LED on initially (to verify hardware works).
     *
     * PC13 LED on the Blue Pill is active low:
     *   ODR bit = 1 → LED off (pin high)
     *   ODR bit = 0 → LED on  (pin low)
     *
     * BRR clears the ODR bit → pin low → LED on.
     */
    LED_PORT->BRR = (1 << LED_PIN);  /* Clear bit → LED on */

    /*
     * Step 5: Configure EXTI for PB1 (button).
     *
     * On STM32F1, EXTI line mapping is done via AFIO->EXTICR[]
     * (not SYSCFG->EXTICR[] like on STM32F0).
     *
     * AFIO->EXTICR[0] controls EXTI lines 0-3.
     * Each line uses 4 bits: 0000=GPIOA, 0001=GPIOB, 0010=GPIOC, ...
     *
     * For EXTI line 1 (BUTTON_PIN=1):
     *   EXTICR[1/4] = EXTICR[0], bits [(1%4)*4 + 3 : (1%4)*4] = bits [7:4]
     *   Set to 0x1 to select GPIOB.
     */
    AFIO->EXTICR[(BUTTON_PIN / 4)] &= ~(0xF << ((BUTTON_PIN % 4) * 4));
    AFIO->EXTICR[(BUTTON_PIN / 4)] |=  (0x1 << ((BUTTON_PIN % 4) * 4));

    /*
     * Step 6: Configure EXTI interrupt for falling edge on line 1.
     *
     * - EXTI->IMR:  Interrupt mask register (1=unmasked/enabled)
     * - EXTI->FTSR: Falling trigger selection register (1=falling edge)
     * - EXTI->RTSR: Rising trigger selection register (0=disabled)
     *
     * We want falling edge only (button press pulls pin low).
     */
    EXTI->IMR  |= (1 << BUTTON_PIN);   /* Unmask EXTI line 1 */
    EXTI->RTSR &= ~(1 << BUTTON_PIN);  /* Disable rising edge */
    EXTI->FTSR |= (1 << BUTTON_PIN);   /* Enable falling edge */

    /*
     * Step 7: Enable EXTI1 interrupt in NVIC.
     *
     * On STM32F1, EXTI line 1 has its own IRQ: EXTI1_IRQn (IRQ 7).
     * (On STM32F0, lines 0-1 share EXTI0_1_IRQn.)
     *
     * Set priority to 0x03 (low priority, higher number = lower priority).
     */
    NVIC_SetPriority(EXTI1_IRQn, 0x03);
    NVIC_EnableIRQ(EXTI1_IRQn);

    /*
     * Step 8: Main loop.
     *
     *   led_blink = 1 → LED blinks (toggle with delay)
     *   led_blink = 0 → LED stays on solid
     *
     * The interrupt handler toggles 'led_blink' on each button press.
     */
    while (1) {
        if (led_blink) {
            /* Blink mode — toggle LED */
            LED_PORT->ODR ^= (1 << LED_PIN);
        }
        else {
            /* Solid mode — LED on solid (active low: clear bit) */
            LED_PORT->BRR = (1 << LED_PIN);
        }

        /* Simple delay for visible blink rate */
        for (volatile uint32_t i = 0; i < 500000; i++) {
            __asm__ volatile ("nop");
        }
    }
}
