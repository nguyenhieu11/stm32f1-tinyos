/**
 * @file    port.c
 * @brief   Cortex-M3 hardware port: context switching, exception priorities.
 */

#include <tinyrtos/kernel/port.h>
#include <tinyrtos/kernel/task.h>
#include <tinyrtos/kernel/scheduler.h>
#include <tinyrtos/kernel/fault.h>

#include "stm32f103x8.h"

uint32_t * rtosk_port_align_stack_pointer(uint32_t *sp) {
    return (uint32_t *)((uint32_t)sp & ~0x7UL);
}

/**
 * SysTick interrupt handler — called every 1 ms by hardware.
 * Name must match the vector table entry (lowercase 'h').
 */
void SysTick_handler(void) {
    rtosk_kernel_tick();
}

/**
 * SVC handler — launches the first task.
 * Restores R4-R11 from the first task's stack, sets PSP,
 * switches Thread mode to use PSP, and returns via EXC_RETURN.
 */
__attribute__((naked)) void SVC_handler(void) {
    __asm volatile (
        "ldr r0, =rtosk_task_get_stack_pointer \n"
        "blx r0                                \n"
        "ldmia r0!, {r4-r11}                   \n"
        "msr psp, r0                           \n"
        "mrs r0, control                       \n"
        "orr r0, r0, #2                        \n"
        "msr control, r0                       \n"
        "isb                                   \n"
        "ldr lr, =0xFFFFFFFD                   \n"
        "bx lr                                 \n"
    );
}

/**
 * PendSV handler — context switch.
 * Saves R4-R11 of current task, calls scheduler, restores R4-R11 of next.
 * No FPU lazy-stacking (Cortex-M3 has no FPU).
 */
__attribute__((naked)) void pend_SV_handler(void) {
    __asm volatile (
        "mrs r0, psp                            \n"
        "stmdb r0!, {r4-r11}                    \n"
        "push {r4, lr}                          \n"
        "ldr r1, =rtosk_task_save_stack_pointer \n"
        "blx r1                                 \n"
        "ldr r1, =rtosk_scheduler_select_next   \n"
        "blx r1                                 \n"
        "ldr r1, =rtosk_task_get_stack_pointer  \n"
        "blx r1                                 \n"
        "pop {r4, lr}                           \n"
        "ldmia r0!, {r4-r11}                    \n"
        "msr psp, r0                            \n"
        "bx lr                                  \n"
    );
}

/**
 * HardFault handler — captures stacked frame, forwards to C handler.
 */
__attribute__((naked)) void hard_fault_handler(void) {
    __asm volatile (
        "tst lr, #4      \n"
        "ite eq          \n"
        "mrseq r0, msp   \n"
        "mrsne r0, psp   \n"
        "mov r1, lr      \n"
        "b rtosk_fault_hardfault_handler \n"
    );
}

void rtosk_port_yield(void) {
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
}

uint32_t rtosk_port_irq_save(void) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    __DSB();
    __ISB();
    return primask;
}

void rtosk_port_irq_restore(uint32_t primask) {
    if ((primask & 1UL) == 0UL) {
        __enable_irq();
    }
}

void rtosk_port_configure_exceptions(void) {
    /* PendSV: lowest priority (0xFF) — runs after all other ISRs */
    NVIC_SetPriority(PendSV_IRQn, 0xFFU);
    /* SysTick: one level above PendSV (0xFE) */
    NVIC_SetPriority(SysTick_IRQn, 0xFEU);
}

void rtosk_port_configure_faults(void) {
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;
}

void rtosk_port_start_first_task(void) {
    __asm volatile ("svc #0");
}
