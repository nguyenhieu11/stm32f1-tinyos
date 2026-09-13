/**
 * @file    fault.c
 * @brief   HardFault handler — blinks LED in distinctive pattern on fault.
 *
 * For initial port, uses LED blink pattern (no UART needed).
 * Fault status registers are still decoded and stored for debugger inspection.
 */

#include <tinyrtos/kernel/fault.h>
#include <tinyrtos/kernel/port.h>

#include "stm32f103x8.h"

/* Fault info stored for debugger inspection */
static volatile uint32_t fault_r0;
static volatile uint32_t fault_r1;
static volatile uint32_t fault_r2;
static volatile uint32_t fault_r3;
static volatile uint32_t fault_r12;
static volatile uint32_t fault_lr;
static volatile uint32_t fault_pc;
static volatile uint32_t fault_xpsr;
static volatile uint32_t fault_cfsr;
static volatile uint32_t fault_hfsr;
static volatile uint32_t fault_bfar;
static volatile uint32_t fault_mmfar;

/**
 * Rapid LED blink to signal HardFault (PC13, active low).
 * Set breakpoint on the __asm volatile ("bkpt") below to inspect fault_* globals.
 */
__attribute__((noreturn)) static void fault_blink_forever(void) {
    /* Enable GPIOC clock if not already on */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    /* PC13 as push-pull output 2 MHz */
    GPIOC->CRH &= ~(0xFUL << ((13 - 8) * 4));
    GPIOC->CRH |=  (GPIO_OUT_PP_2MHZ << ((13 - 8) * 4));

    for(;;) {
        /* Rapid blink pattern: 100ms on, 100ms off */
        GPIOC->ODR ^= (1UL << 13);
        for(volatile uint32_t i = 0; i < 200000; i++) {
            __asm volatile ("nop");
        }
    }
}

__attribute__((noreturn)) void rtosk_fault_hardfault_handler(uint32_t * stacked_registers, uint32_t exc_return) {
    __disable_irq();

    /* Save stacked registers for debugger inspection */
    fault_r0    = stacked_registers[0];
    fault_r1    = stacked_registers[1];
    fault_r2    = stacked_registers[2];
    fault_r3    = stacked_registers[3];
    fault_r12   = stacked_registers[4];
    fault_lr    = stacked_registers[5];
    fault_pc    = stacked_registers[6];
    fault_xpsr  = stacked_registers[7];

    /* Save fault status registers */
    fault_cfsr  = SCB->CFSR;
    fault_hfsr  = SCB->HFSR;
    fault_bfar  = SCB->BFAR;
    fault_mmfar = SCB->MMFAR;

    /* Clear fault status by writing 1s */
    SCB->CFSR = fault_cfsr;
    SCB->HFSR = fault_hfsr;

    (void)exc_return;

    /* Blink LED to signal fault, with breakpoint opportunity */
    fault_blink_forever();
}
