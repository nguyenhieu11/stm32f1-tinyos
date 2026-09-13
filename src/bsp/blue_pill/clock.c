/**
 * @file    clock.c
 * @brief   System clock configuration: HSE (8 MHz) + PLL (×9) = 72 MHz.
 *
 * Blue Pill boards typically have an 8 MHz HSE crystal.
 * APB1 prescaler = /2 (APB1 max 36 MHz), APB2 = /1, AHB = /1.
 */

#include "clock.h"
#include "stm32f103x8.h"

/* System core clock frequency, set by clock_init() */
uint32_t SystemCoreClock = 8000000UL;  /* Default: 8 MHz HSI */

void clock_init(void) {
    /* 1. Enable HSE (external 8 MHz crystal) */
    RCC->CR |= (1UL << 16);  /* HSEON */
    while(!(RCC->CR & (1UL << 17)));  /* Wait for HSERD */

    /* 2. Set flash latency for 72 MHz (2 wait states) */
    FLASH_ACR_REG = FLASH_ACR_LATENCY_2;

    /* 3. Configure PLL: HSE as source, multiply by 9 → 72 MHz */
    /*    PLLSRC = bit 16 (HSE), PLLMUL[21:18] = 0b0111 (×9) */
    RCC->CFGR |= (1UL << 16);        /* PLLSRC = HSE */
    RCC->CFGR |= (0x7UL << 18);      /* PLLMUL = ×9 */

    /* 4. Set bus prescalers */
    /*    AHB  prescaler = /1  (HPRE[7:4]  = 0b0000) — default */
    /*    APB1 prescaler = /2 (PPRE1[10:8] = 0b100) */
    /*    APB2 prescaler = /1 (PPRE2[13:11] = 0b000) — default */
    RCC->CFGR |= (0x4UL << 8);       /* APB1 = /2 */

    /* 5. Enable PLL */
    RCC->CR |= (1UL << 24);          /* PLLON */
    while(!(RCC->CR & (1UL << 25)));  /* Wait for PLLRDY */

    /* 6. Switch system clock to PLL */
    /*    SW[1:0] = 0b10 (PLL selected) */
    RCC->CFGR |= (0x2UL << 0);       /* SW = PLL */
    while((RCC->CFGR & (0x3UL << 2)) != (0x2UL << 2));  /* Wait for SWS = PLL */

    /* 7. Update SystemCoreClock */
    SystemCoreClock = 72000000UL;
}
