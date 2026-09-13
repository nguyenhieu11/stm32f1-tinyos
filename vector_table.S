/*
 * Vector table for STM32F103C8T6 (medium-density, Cortex-M3).
 *
 * Based on Vivonomicon's "Bare Metal STM32 Programming Part 2"
 * adapted for the STM32F103C8T6.
 *
 * Reference: RM0008 Section 9.1.2 "Interrupt and exception vectors"
 * The vector table must be placed at the start of Flash (0x08000000).
 *
 * All interrupt handlers are weak-aliased to 'default_interrupt_handler'
 * (an infinite loop). Override any handler by defining a function with
 * the same name in C code.
 *
 * Open source under the MIT License
 */

.syntax unified
.cpu cortex-m3
.fpu softvfp
.thumb

.global vtable
.global default_interrupt_handler

/*
 * The vector table for STM32F103C8T6 (medium-density).
 * 68 peripheral IRQs + 15 system exceptions + 1 reserved word = 84 entries.
 * See RM0008 Table 60 (medium-density vector table).
 */
.type vtable, %object
.section .vector_table,"a",%progbits
vtable:
    /* Cortex-M3 system exceptions (positions 0-15) */
    .word _estack                  /* 0x00: Initial Stack Pointer */
    .word reset_handler            /* 0x04: Reset */
    .word NMI_handler              /* 0x08: Non-Maskable Interrupt */
    .word hard_fault_handler       /* 0x0C: Hard Fault */
    .word mem_manage_handler       /* 0x10: Memory Management */
    .word bus_fault_handler        /* 0x14: Bus Fault */
    .word usage_fault_handler      /* 0x18: Usage Fault */
    .word 0                        /* 0x1C: Reserved */
    .word 0                        /* 0x20: Reserved */
    .word 0                        /* 0x24: Reserved */
    .word 0                        /* 0x28: Reserved */
    .word SVC_handler              /* 0x2C: SVCall */
    .word debug_mon_handler        /* 0x30: Debug Monitor */
    .word 0                        /* 0x34: Reserved */
    .word pend_SV_handler          /* 0x38: PendSV */
    .word SysTick_handler          /* 0x3C: SysTick */

    /* STM32F103C8T6 peripheral IRQs (positions 16-67) */
    .word WWDG_handler             /* 0x40: Window Watchdog */
    .word PVD_handler              /* 0x44: PVD through EXTI */
    .word TAMPER_handler           /* 0x48: Tamper */
    .word RTC_handler              /* 0x4C: RTC */
    .word FLASH_handler            /* 0x50: Flash */
    .word RCC_handler              /* 0x54: RCC */
    .word EXTI0_handler            /* 0x58: EXTI Line 0 */
    .word EXTI1_handler            /* 0x5C: EXTI Line 1 */
    .word EXTI2_handler            /* 0x60: EXTI Line 2 */
    .word EXTI3_handler            /* 0x64: EXTI Line 3 */
    .word EXTI4_handler            /* 0x68: EXTI Line 4 */
    .word DMA1_Channel1_handler    /* 0x6C: DMA1 Channel 1 */
    .word DMA1_Channel2_handler    /* 0x70: DMA1 Channel 2 */
    .word DMA1_Channel3_handler    /* 0x74: DMA1 Channel 3 */
    .word DMA1_Channel4_handler    /* 0x78: DMA1 Channel 4 */
    .word DMA1_Channel5_handler    /* 0x7C: DMA1 Channel 5 */
    .word DMA1_Channel6_handler    /* 0x80: DMA1 Channel 6 */
    .word DMA1_Channel7_handler    /* 0x84: DMA1 Channel 7 */
    .word ADC1_2_handler           /* 0x88: ADC1 and ADC2 */
    .word USB_HP_CAN1_TX_handler   /* 0x8C: USB High Priority / CAN1 TX */
    .word USB_LP_CAN1_RX0_handler  /* 0x90: USB Low Priority / CAN1 RX0 */
    .word CAN1_RX1_handler         /* 0x94: CAN1 RX1 */
    .word CAN1_SCE_handler         /* 0x98: CAN1 SCE */
    .word EXTI9_5_handler          /* 0x9C: EXTI Lines 5-9 */
    .word TIM1_BRK_handler         /* 0xA0: TIM1 Break */
    .word TIM1_UP_handler          /* 0xA4: TIM1 Update */
    .word TIM1_TRG_COM_handler     /* 0xA8: TIM1 Trigger and Commutation */
    .word TIM1_CC_handler          /* 0xAC: TIM1 Capture Compare */
    .word TIM2_handler             /* 0xB0: TIM2 */
    .word TIM3_handler             /* 0xB4: TIM3 */
    .word TIM4_handler             /* 0xB8: TIM4 */
    .word I2C1_EV_handler          /* 0xBC: I2C1 Event */
    .word I2C1_ER_handler          /* 0xC0: I2C1 Error */
    .word I2C2_EV_handler          /* 0xC4: I2C2 Event */
    .word I2C2_ER_handler          /* 0xC8: I2C2 Error */
    .word SPI1_handler             /* 0xCC: SPI1 */
    .word SPI2_handler             /* 0xD0: SPI2 */
    .word USART1_handler           /* 0xD4: USART1 */
    .word USART2_handler           /* 0xD8: USART2 */
    .word USART3_handler           /* 0xDC: USART3 */
    .word EXTI15_10_handler        /* 0xE0: EXTI Lines 10-15 */
    .word RTCAlarm_handler         /* 0xE4: RTC Alarm through EXTI */
    .word USBWakeup_handler        /* 0xE8: USB Wakeup from suspend */
.size vtable, .-vtable

/*
 * Setup weak aliases for each exception handler to the
 * default one. These can be overridden in C code by
 * defining a function with the same name.
 */

/* Cortex-M3 system exceptions */
.weak NMI_handler
.thumb_set NMI_handler,default_interrupt_handler
.weak hard_fault_handler
.thumb_set hard_fault_handler,default_interrupt_handler
.weak mem_manage_handler
.thumb_set mem_manage_handler,default_interrupt_handler
.weak bus_fault_handler
.thumb_set bus_fault_handler,default_interrupt_handler
.weak usage_fault_handler
.thumb_set usage_fault_handler,default_interrupt_handler
.weak SVC_handler
.thumb_set SVC_handler,default_interrupt_handler
.weak debug_mon_handler
.thumb_set debug_mon_handler,default_interrupt_handler
.weak pend_SV_handler
.thumb_set pend_SV_handler,default_interrupt_handler
.weak SysTick_handler
.thumb_set SysTick_handler,default_interrupt_handler

/* STM32F103C8T6 peripheral IRQs */
.weak WWDG_handler
.thumb_set WWDG_handler,default_interrupt_handler
.weak PVD_handler
.thumb_set PVD_handler,default_interrupt_handler
.weak TAMPER_handler
.thumb_set TAMPER_handler,default_interrupt_handler
.weak RTC_handler
.thumb_set RTC_handler,default_interrupt_handler
.weak FLASH_handler
.thumb_set FLASH_handler,default_interrupt_handler
.weak RCC_handler
.thumb_set RCC_handler,default_interrupt_handler
.weak EXTI0_handler
.thumb_set EXTI0_handler,default_interrupt_handler
.weak EXTI1_handler
.thumb_set EXTI1_handler,default_interrupt_handler
.weak EXTI2_handler
.thumb_set EXTI2_handler,default_interrupt_handler
.weak EXTI3_handler
.thumb_set EXTI3_handler,default_interrupt_handler
.weak EXTI4_handler
.thumb_set EXTI4_handler,default_interrupt_handler
.weak DMA1_Channel1_handler
.thumb_set DMA1_Channel1_handler,default_interrupt_handler
.weak DMA1_Channel2_handler
.thumb_set DMA1_Channel2_handler,default_interrupt_handler
.weak DMA1_Channel3_handler
.thumb_set DMA1_Channel3_handler,default_interrupt_handler
.weak DMA1_Channel4_handler
.thumb_set DMA1_Channel4_handler,default_interrupt_handler
.weak DMA1_Channel5_handler
.thumb_set DMA1_Channel5_handler,default_interrupt_handler
.weak DMA1_Channel6_handler
.thumb_set DMA1_Channel6_handler,default_interrupt_handler
.weak DMA1_Channel7_handler
.thumb_set DMA1_Channel7_handler,default_interrupt_handler
.weak ADC1_2_handler
.thumb_set ADC1_2_handler,default_interrupt_handler
.weak USB_HP_CAN1_TX_handler
.thumb_set USB_HP_CAN1_TX_handler,default_interrupt_handler
.weak USB_LP_CAN1_RX0_handler
.thumb_set USB_LP_CAN1_RX0_handler,default_interrupt_handler
.weak CAN1_RX1_handler
.thumb_set CAN1_RX1_handler,default_interrupt_handler
.weak CAN1_SCE_handler
.thumb_set CAN1_SCE_handler,default_interrupt_handler
.weak EXTI9_5_handler
.thumb_set EXTI9_5_handler,default_interrupt_handler
.weak TIM1_BRK_handler
.thumb_set TIM1_BRK_handler,default_interrupt_handler
.weak TIM1_UP_handler
.thumb_set TIM1_UP_handler,default_interrupt_handler
.weak TIM1_TRG_COM_handler
.thumb_set TIM1_TRG_COM_handler,default_interrupt_handler
.weak TIM1_CC_handler
.thumb_set TIM1_CC_handler,default_interrupt_handler
.weak TIM2_handler
.thumb_set TIM2_handler,default_interrupt_handler
.weak TIM3_handler
.thumb_set TIM3_handler,default_interrupt_handler
.weak TIM4_handler
.thumb_set TIM4_handler,default_interrupt_handler
.weak I2C1_EV_handler
.thumb_set I2C1_EV_handler,default_interrupt_handler
.weak I2C1_ER_handler
.thumb_set I2C1_ER_handler,default_interrupt_handler
.weak I2C2_EV_handler
.thumb_set I2C2_EV_handler,default_interrupt_handler
.weak I2C2_ER_handler
.thumb_set I2C2_ER_handler,default_interrupt_handler
.weak SPI1_handler
.thumb_set SPI1_handler,default_interrupt_handler
.weak SPI2_handler
.thumb_set SPI2_handler,default_interrupt_handler
.weak USART1_handler
.thumb_set USART1_handler,default_interrupt_handler
.weak USART2_handler
.thumb_set USART2_handler,default_interrupt_handler
.weak USART3_handler
.thumb_set USART3_handler,default_interrupt_handler
.weak EXTI15_10_handler
.thumb_set EXTI15_10_handler,default_interrupt_handler
.weak RTCAlarm_handler
.thumb_set RTCAlarm_handler,default_interrupt_handler
.weak USBWakeup_handler
.thumb_set USBWakeup_handler,default_interrupt_handler

/*
 * A 'Default' interrupt handler. This is where interrupts
 * which are not otherwise configured will go.
 * It is an infinite loop, because...well, we weren't
 * expecting the interrupt, so what can we do?
 */
.section .text.default_interrupt_handler,"ax",%progbits
default_interrupt_handler:
    default_interrupt_loop:
      B default_interrupt_loop
.size default_interrupt_handler, .-default_interrupt_handler
