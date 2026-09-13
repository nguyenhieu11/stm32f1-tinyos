/**
 * @file    fault.c
 * @brief   HardFault handler — blinks LED on fault, stores registers for debugger.
 */

#include <tinyrtos/kernel/fault.h>
#include "stm32f103x8.h"

static volatile uint32_t fault_r0, fault_r1, fault_r2, fault_r3;
static volatile uint32_t fault_r12, fault_lr, fault_pc, fault_xpsr;
static volatile uint32_t fault_cfsr, fault_hfsr, fault_bfar, fault_mmfar;

__attribute__((noreturn)) static void fault_blink_forever(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(0xFUL << ((13 - 8) * 4));
    GPIOC->CRH |=  (GPIO_OUT_PP_2MHZ << ((13 - 8) * 4));
    for (;;) {
        GPIOC->ODR ^= (1UL << 13);
        for (volatile uint32_t i = 0; i < 200000; i++) {
            __asm volatile ("nop");
        }
    }
}

__attribute__((noreturn)) void rtosk_fault_hardfault_handler(uint32_t * stacked_registers, uint32_t exc_return) {
    __disable_irq();
    fault_r0   = stacked_registers[0]; fault_r1   = stacked_registers[1];
    fault_r2   = stacked_registers[2]; fault_r3   = stacked_registers[3];
    fault_r12  = stacked_registers[4]; fault_lr   = stacked_registers[5];
    fault_pc   = stacked_registers[6]; fault_xpsr = stacked_registers[7];
    fault_cfsr = SCB->CFSR; fault_hfsr = SCB->HFSR;
    fault_bfar = SCB->BFAR; fault_mmfar = SCB->MMFAR;
    SCB->CFSR = fault_cfsr; SCB->HFSR = fault_hfsr;
    (void)exc_return;
    fault_blink_forever();
}
