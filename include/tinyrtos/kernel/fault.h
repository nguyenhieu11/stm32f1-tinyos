/**
 * @file    fault.h
 * @brief   HardFault handler with register dump.
 */

#ifndef _RTOSK_FAULT_H_
#define _RTOSK_FAULT_H_

#include <stdint.h>

__attribute__((noreturn)) void rtosk_fault_hardfault_handler(uint32_t * stacked_registers, uint32_t exc_return);

#endif /* _RTOSK_FAULT_H_ */
