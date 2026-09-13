/**
 * @file    port.h
 * @brief   Cortex-M3 hardware port for TinyRTOS.
 *
 * Low-level interface between the RTOS and the ARM Cortex-M3 hardware.
 * Handles exception priorities, context switching, and interrupt control.
 */

#ifndef _RTOSK_PORT_H_
#define _RTOSK_PORT_H_

#include <stdint.h>

/* Thumb bit for xPSR in exception stack frame (bit 24 must be 1) */
#define RTOSK_XPSR_T_BIT 0x01000000UL

/* EXC_RETURN: return to Thread mode using PSP */
#define RTOSK_EXC_RETURN_PSP 0xFFFFFFFDUL

/* Port layer API */
uint32_t * rtosk_port_align_stack_pointer(uint32_t * sp);
void rtosk_port_start_first_task(void);
void rtosk_port_yield(void);
uint32_t rtosk_port_irq_save(void);
void rtosk_port_irq_restore(uint32_t primask);
void rtosk_port_configure_exceptions(void);
void rtosk_port_configure_faults(void);

/* Called from SysTick_Handler (defined in kernel.c) */
void rtosk_kernel_tick(void);

#endif /* _RTOSK_PORT_H_ */
