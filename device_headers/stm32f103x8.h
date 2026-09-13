/**
 * Minimal device header for STM32F103C8T6 (medium-density, Cortex-M3).
 *
 * Contains only the peripheral register definitions needed for
 * Part 3: GPIO (LEDs and buttons).
 *
 * Reference: RM0008 (STM32F10xxx reference manual)
 *   - Section 7: RCC (Reset and Clock Control)
 *   - Section 9: GPIO (General-purpose I/Os)
 *
 * Based on Vivonomicon's "Bare Metal STM32 Programming Part 3"
 * adapted for STM32F103C8T6.
 *
 * Open source under the MIT License
 */

#ifndef _STM32F103X8_H
#define _STM32F103X8_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Cortex-M3 core configuration (required by some CMSIS headers)     */
/* ------------------------------------------------------------------ */
#define __CM3_REV                 0x0200  /* Core Revision r2p0 */
#define __MPU_PRESENT             0       /* STM32F103 does not provide MPU */
#define __NVIC_PRIO_BITS          4       /* STM32F103 uses 4 bits for priority */
#define __Vendor_SysTickConfig    0

/* ------------------------------------------------------------------ */
/*  Peripheral register access macros                                  */
/* ------------------------------------------------------------------ */
#define __IO  volatile
#define __I   volatile const
#define __O   volatile

/* ------------------------------------------------------------------ */
/*  Interrupt numbers (for completeness, not used in Part 3)           */
/* ------------------------------------------------------------------ */
typedef enum {
    /* Cortex-M3 processor exceptions */
    NonMaskableInt_IRQn     = -14,
    HardFault_IRQn          = -13,
    MemoryManagement_IRQn   = -12,
    BusFault_IRQn           = -11,
    UsageFault_IRQn         = -10,
    SVCall_IRQn             = -5,
    DebugMonitor_IRQn       = -4,
    PendSV_IRQn             = -2,
    SysTick_IRQn            = -1,
    /* STM32F103 peripheral interrupts */
    WWDG_IRQn               = 0,
    PVD_IRQn                = 1,
    TAMPER_IRQn             = 2,
    RTC_IRQn                = 3,
    FLASH_IRQn              = 4,
    RCC_IRQn                = 5,
    EXTI0_IRQn              = 6,
    EXTI1_IRQn              = 7,
    EXTI2_IRQn              = 8,
    EXTI3_IRQn              = 9,
    EXTI4_IRQn              = 10,
    DMA1_Channel1_IRQn      = 11,
    DMA1_Channel2_IRQn      = 12,
    DMA1_Channel3_IRQn      = 13,
    DMA1_Channel4_IRQn      = 14,
    DMA1_Channel5_IRQn      = 15,
    DMA1_Channel6_IRQn      = 16,
    DMA1_Channel7_IRQn      = 17,
    ADC1_2_IRQn             = 18,
    USB_HP_CAN1_TX_IRQn     = 19,
    USB_LP_CAN1_RX0_IRQn    = 20,
    CAN1_RX1_IRQn           = 21,
    CAN1_SCE_IRQn           = 22,
    EXTI9_5_IRQn            = 23,
    TIM1_BRK_IRQn           = 24,
    TIM1_UP_IRQn            = 25,
    TIM1_TRG_COM_IRQn       = 26,
    TIM1_CC_IRQn            = 27,
    TIM2_IRQn               = 28,
    TIM3_IRQn               = 29,
    TIM4_IRQn               = 30,
    I2C1_EV_IRQn            = 31,
    I2C1_ER_IRQn            = 32,
    I2C2_EV_IRQn            = 33,
    I2C2_ER_IRQn            = 34,
    SPI1_IRQn               = 35,
    SPI2_IRQn               = 36,
    USART1_IRQn             = 37,
    USART2_IRQn             = 38,
    USART3_IRQn             = 39,
    EXTI15_10_IRQn          = 40,
    RTCAlarm_IRQn           = 41,
    USBWakeup_IRQn          = 42,
} IRQn_Type;

/* ================================================================== */
/*  RCC (Reset and Clock Control)                                      */
/*  Base: 0x40021000                                                   */
/*  Reference: RM0008 Section 7.3                                      */
/* ================================================================== */

typedef struct {
    __IO uint32_t CR;         /* 0x00: Clock control register */
    __IO uint32_t CFGR;       /* 0x04: Clock configuration register */
    __IO uint32_t CIR;        /* 0x08: Clock interrupt register */
    __IO uint32_t APB2RSTR;   /* 0x0C: APB2 peripheral reset register */
    __IO uint32_t APB1RSTR;   /* 0x10: APB1 peripheral reset register */
    __IO uint32_t AHBENR;     /* 0x14: AHB peripheral clock enable register */
    __IO uint32_t APB2ENR;    /* 0x18: APB2 peripheral clock enable register */
    __IO uint32_t APB1ENR;    /* 0x1C: APB1 peripheral clock enable register */
    __IO uint32_t BDCR;       /* 0x20: Backup domain control register */
    __IO uint32_t CSR;        /* 0x24: Control/status register */
} RCC_TypeDef;

#define RCC_BASE              (0x40021000UL)
#define RCC                   ((RCC_TypeDef *) RCC_BASE)

/* --- RCC_APB2ENR bits --- */
/* GPIO clocks are on APB2 for STM32F1 (unlike F0 which uses AHB) */
#define RCC_APB2ENR_AFIOEN    (1UL << 0)   /* Alternate function IO clock enable */
#define RCC_APB2ENR_IOPAEN    (1UL << 2)   /* IO port A clock enable */
#define RCC_APB2ENR_IOPBEN    (1UL << 3)   /* IO port B clock enable */
#define RCC_APB2ENR_IOPCEN    (1UL << 4)   /* IO port C clock enable */
#define RCC_APB2ENR_IOPDEN    (1UL << 5)   /* IO port D clock enable */
#define RCC_APB2ENR_IOPEEN    (1UL << 6)   /* IO port E clock enable */
#define RCC_APB2ENR_ADC1EN    (1UL << 9)   /* ADC 1 interface clock enable */
#define RCC_APB2ENR_ADC2EN    (1UL << 10)  /* ADC 2 interface clock enable */
#define RCC_APB2ENR_TIM1EN    (1UL << 11)  /* TIM1 clock enable */
#define RCC_APB2ENR_SPI1EN    (1UL << 12)  /* SPI1 clock enable */
#define RCC_APB2ENR_USART1EN  (1UL << 14)  /* USART1 clock enable */

/* ================================================================== */
/*  GPIO (General-Purpose I/Os)                                        */
/*  Base addresses: GPIOA=0x40010800, GPIOB=0x40010C00, GPIOC=0x40011000 */
/*  Reference: RM0008 Section 9.2                                      */
/*                                                                     */
/*  STM32F1 GPIO uses CRL/CRH registers (4 bits per pin):             */
/*    CNF[1:0] MODE[1:0]                                               */
/*                                                                     */
/*  Input mode (MODE=00):                                              */
/*    CNF=00: Analog input                                             */
/*    CNF=01: Floating input (reset state)                             */
/*    CNF=10: Input with pull-up/pull-down                             */
/*    CNF=11: Reserved                                                 */
/*                                                                     */
/*  Output mode (MODE=01: 10MHz, 10: 2MHz, 11: 50MHz):                */
/*    CNF=00: General-purpose output push-pull                         */
/*    CNF=01: General-purpose output open-drain                        */
/*    CNF=10: Alternate function output push-pull                      */
/*    CNF=11: Alternate function output open-drain                     */
/* ================================================================== */

typedef struct {
    __IO uint32_t CRL;    /* 0x00: Configuration register low (pins 0-7) */
    __IO uint32_t CRH;    /* 0x04: Configuration register high (pins 8-15) */
    __IO uint32_t IDR;    /* 0x08: Input data register */
    __IO uint32_t ODR;    /* 0x0C: Output data register */
    __IO uint32_t BSRR;   /* 0x10: Bit set/reset register */
    __IO uint32_t BRR;    /* 0x14: Bit reset register */
    __IO uint32_t LCKR;   /* 0x18: Lock register */
} GPIO_TypeDef;

#define GPIOA_BASE            (0x40010800UL)
#define GPIOB_BASE            (0x40010C00UL)
#define GPIOC_BASE            (0x40011000UL)

#define GPIOA                 ((GPIO_TypeDef *) GPIOA_BASE)
#define GPIOB                 ((GPIO_TypeDef *) GPIOB_BASE)
#define GPIOC                 ((GPIO_TypeDef *) GPIOC_BASE)

/* --- CRL/CRH configuration values (4 bits per pin) --- */
/* Mode bits [1:0] */
#define GPIO_MODE_INPUT       0x0  /* Input mode (reset state) */
#define GPIO_MODE_OUT_10MHZ   0x1  /* Output mode, max speed 10 MHz */
#define GPIO_MODE_OUT_2MHZ    0x2  /* Output mode, max speed 2 MHz */
#define GPIO_MODE_OUT_50MHZ   0x3  /* Output mode, max speed 50 MHz */

/* CNF bits [3:2] in input mode */
#define GPIO_CNF_ANALOG       0x0  /* Analog input */
#define GPIO_CNF_FLOATING     0x4  /* Floating input */
#define GPIO_CNF_PULL         0x8  /* Input with pull-up/pull-down */

/* CNF bits [3:2] in output mode */
#define GPIO_CNF_OUT_PP       0x0  /* General-purpose output push-pull */
#define GPIO_CNF_OUT_OD       0x4  /* General-purpose output open-drain */
#define GPIO_CNF_AF_PP        0x8  /* Alternate function output push-pull */
#define GPIO_CNF_AF_OD        0xC  /* Alternate function output open-drain */

/* --- Convenience macros for common configurations --- */
/* Input: floating (no pull) */
#define GPIO_IN_FLOATING      (GPIO_CNF_FLOATING | GPIO_MODE_INPUT)
/* Input: pull-up or pull-down (ODR selects up/down) */
#define GPIO_IN_PULL          (GPIO_CNF_PULL | GPIO_MODE_INPUT)
/* Input: analog */
#define GPIO_IN_ANALOG        (GPIO_CNF_ANALOG | GPIO_MODE_INPUT)
/* Output: push-pull 2 MHz */
#define GPIO_OUT_PP_2MHZ      (GPIO_CNF_OUT_PP | GPIO_MODE_OUT_2MHZ)
/* Output: push-pull 10 MHz */
#define GPIO_OUT_PP_10MHZ     (GPIO_CNF_OUT_PP | GPIO_MODE_OUT_10MHZ)
/* Output: push-pull 50 MHz */
#define GPIO_OUT_PP_50MHZ     (GPIO_CNF_OUT_PP | GPIO_MODE_OUT_50MHZ)
/* Output: open-drain 2 MHz */
#define GPIO_OUT_OD_2MHZ      (GPIO_CNF_OUT_OD | GPIO_MODE_OUT_2MHZ)
/* Alternate function: push-pull 50 MHz */
#define GPIO_AF_PP_50MHZ      (GPIO_CNF_AF_PP | GPIO_MODE_OUT_50MHZ)

/* ================================================================== */
/*  AFIO (Alternate Function I/O)                                      */
/*  Base: 0x40010000                                                   */
/*  Reference: RM0008 Section 9.4                                      */
/*                                                                     */
/*  On STM32F1, AFIO is used to remap pins and to select which GPIO   */
/*  port is connected to each EXTI line. (STM32F0/L0 use SYSCFG.)     */
/* ================================================================== */

typedef struct {
    __IO uint32_t EVCR;       /* 0x00: Event control register */
    __IO uint32_t MAPR;       /* 0x04: AF remap and debug I/O config register */
    __IO uint32_t EXTICR[4];  /* 0x08-0x14: External interrupt configuration registers */
    __IO uint32_t RESERVED0;  /* 0x18: Reserved */
    __IO uint32_t MAPR2;      /* 0x1C: AF remap and debug I/O config register 2 */
} AFIO_TypeDef;

#define AFIO_BASE             (0x40010000UL)
#define AFIO                  ((AFIO_TypeDef *) AFIO_BASE)

/* ================================================================== */
/*  EXTI (External Interrupt/Event Controller)                         */
/*  Base: 0x40010400                                                   */
/*  Reference: RM0008 Section 10.3                                     */
/* ================================================================== */

typedef struct {
    __IO uint32_t IMR;        /* 0x00: Interrupt mask register */
    __IO uint32_t EMR;        /* 0x04: Event mask register */
    __IO uint32_t RTSR;       /* 0x08: Rising trigger selection register */
    __IO uint32_t FTSR;       /* 0x0C: Falling trigger selection register */
    __IO uint32_t SWIER;      /* 0x10: Software interrupt event register */
    __IO uint32_t PR;         /* 0x14: Pending register */
} EXTI_TypeDef;

#define EXTI_BASE             (0x40010400UL)
#define EXTI                  ((EXTI_TypeDef *) EXTI_BASE)

/* ================================================================== */
/*  NVIC (Nested Vectored Interrupt Controller)                        */
/*  Cortex-M3 core peripheral at 0xE000E100                            */
/*  Reference: ARM Cortex-M3 Technical Reference Manual                */
/* ================================================================== */

/* NVIC registers (only the ones we need) */
typedef struct {
    __IO uint32_t ISER[8];    /* 0x00-0x1C: Interrupt Set Enable Registers */
    __IO uint32_t RESERVED0[24];
    __IO uint32_t ICER[8];    /* 0x80-0x9C: Interrupt Clear Enable Registers */
    __IO uint32_t RESERVED1[24];
    __IO uint32_t ISPR[8];    /* 0x100-0x11C: Interrupt Set Pending Registers */
    __IO uint32_t RESERVED2[24];
    __IO uint32_t ICPR[8];    /* 0x180-0x19C: Interrupt Clear Pending Registers */
    __IO uint32_t RESERVED3[24];
    __IO uint32_t IABR[8];    /* 0x200-0x21C: Interrupt Active Bit Registers */
    __IO uint32_t RESERVED4[56];
    __IO uint8_t  IP[240];    /* 0x300-0x3EF: Interrupt Priority Registers */
} NVIC_TypeDef;

#define NVIC_BASE             (0xE000E100UL)
#define NVIC                  ((NVIC_TypeDef *) NVIC_BASE)

/* System Control Block (for priority configuration) */
#define SCB_BASE              (0xE000ED00UL)
#define SCB                   ((SCB_Type *) SCB_BASE)

typedef struct {
    __I  uint32_t CPUID;        /* 0x00: CPUID base register */
    __IO uint32_t ICSR;         /* 0x04: Interrupt control and state register */
    __IO uint32_t VTOR;         /* 0x08: Vector table offset register */
    __IO uint32_t AIRCR;        /* 0x0C: Application interrupt and reset control register */
    __IO uint32_t SCR;          /* 0x10: System control register */
    __IO uint32_t CCR;          /* 0x14: Configuration and control register */
    __IO uint8_t  SHP[12];      /* 0x18: System handler priority registers */
    __IO uint32_t SHCSR;        /* 0x24: System handler control and state register */
    __IO uint32_t CFSR;         /* 0x28: Configurable fault status register */
    __IO uint32_t HFSR;         /* 0x2C: Hard fault status register */
    __IO uint32_t DFSR;         /* 0x30: Debug fault status register */
    __IO uint32_t MMFAR;        /* 0x34: MemManage fault address register */
    __IO uint32_t BFAR;         /* 0x38: Bus fault address register */
    __IO uint32_t AFSR;         /* 0x3C: Auxiliary fault status register */
} SCB_Type;

/* --- SCB bit definitions --- */
#define SCB_ICSR_PENDSVSET_Msk      (1UL << 28)
#define SCB_CCR_DIV_0_TRP_Msk       (1UL << 4)
#define SCB_CCR_UNALIGN_TRP_Msk     (1UL << 3)

/* ================================================================== */
/*  SysTick (System Timer)                                             */
/*  Cortex-M3 core peripheral at 0xE000E010                            */
/*  Reference: ARM Cortex-M3 Technical Reference Manual                */
/* ================================================================== */

typedef struct {
    __IO uint32_t CTRL;     /* 0x00: SysTick control and status register */
    __IO uint32_t LOAD;     /* 0x04: SysTick reload value register */
    __IO uint32_t VAL;      /* 0x08: SysTick current value register */
    __I  uint32_t CALIB;    /* 0x0C: SysTick calibration register */
} SysTick_Type;

#define SysTick             ((SysTick_Type *) 0xE000E010UL)

#define SysTick_CTRL_ENABLE_Msk     (1UL << 0)
#define SysTick_CTRL_TICKINT_Msk    (1UL << 1)
#define SysTick_CTRL_CLKSOURCE_Msk  (1UL << 2)
#define SysTick_CTRL_COUNTFLAG_Msk  (1UL << 16)

/* ================================================================== */
/*  FLASH (embedded flash memory controller)                           */
/*  Base: 0x40022000                                                   */
/*  Reference: RM0008 Section 3.3                                      */
/* ================================================================== */

#define FLASH_ACR_REG       (*((volatile uint32_t *) 0x40022000UL))
#define FLASH_ACR_LATENCY_0  0x00UL   /* 0 wait states (0 < SYSCLK <= 24 MHz) */
#define FLASH_ACR_LATENCY_1  0x01UL   /* 1 wait state  (24 < SYSCLK <= 48 MHz) */
#define FLASH_ACR_LATENCY_2  0x02UL   /* 2 wait states (48 < SYSCLK <= 72 MHz) */

/* ================================================================== */
/*  CMSIS-style compiler intrinsics for bare-metal                     */
/* ================================================================== */

static inline void __disable_irq(void) {
    __asm volatile ("cpsid i" ::: "memory");
}

static inline void __enable_irq(void) {
    __asm volatile ("cpsie i" ::: "memory");
}

static inline void __DSB(void) {
    __asm volatile ("dsb 0xF" ::: "memory");
}

static inline void __ISB(void) {
    __asm volatile ("isb 0xF" ::: "memory");
}

static inline uint32_t __get_PRIMASK(void) {
    uint32_t result;
    __asm volatile ("MRS %0, primask" : "=r"(result));
    return result;
}

static inline void __set_PRIMASK(uint32_t primask) {
    __asm volatile ("MSR primask, %0" :: "r"(primask) : "memory");
}

/* ================================================================== */
/*  SystemCoreClock — global variable for system clock frequency       */
/*  Set by clock_init() to actual frequency in Hz.                     */
/* ================================================================== */

extern uint32_t SystemCoreClock;

/* --- NVIC helper functions (inline, CMSIS-compatible) --- */

static inline void NVIC_EnableIRQ(IRQn_Type IRQn) {
    NVIC->ISER[(uint32_t)IRQn >> 5] = (1UL << ((uint32_t)IRQn & 0x1F));
}

static inline void NVIC_DisableIRQ(IRQn_Type IRQn) {
    NVIC->ICER[(uint32_t)IRQn >> 5] = (1UL << ((uint32_t)IRQn & 0x1F));
}

static inline void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority) {
    if ((int32_t)IRQn >= 0) {
        NVIC->IP[(uint32_t)IRQn] = (uint8_t)((priority << (8U - __NVIC_PRIO_BITS)) & 0xFFUL);
    } else {
        SCB->SHP[(((uint32_t)IRQn) & 0xFUL) - 4UL] =
            (uint8_t)((priority << (8U - __NVIC_PRIO_BITS)) & 0xFFUL);
    }
}

#endif /* _STM32F103X8_H */
