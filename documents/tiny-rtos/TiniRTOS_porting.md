# TinyRTOS v0.1.0 — Porting Guide

## Overview

TinyRTOS is a preemptive priority-based RTOS kernel ported to STM32F103C8T6 (Cortex-M3, Blue Pill). The kernel logic is adapted from [Ian Wilkey's tinyrtos](https://github.com/Ian-Wilkey/tinyrtos) which targets Cortex-M7 (Nucleo-F756ZG) with PlatformIO + CMSIS.

This port runs on bare-metal with a Makefile-based build system, no HAL or CMSIS framework dependency.

## Source / Reference Comparison

| Aspect | Reference tinyrtos | This port |
|--------|-------------------|-----------|
| Target MCU | STM32F756ZG (Cortex-M7) | STM32F103C8T6 (Cortex-M3) |
| Build system | PlatformIO + CMSIS framework | Bare-metal Makefile + arm-none-eabi-gcc |
| System clock | Provided by CMSIS (`SystemCoreClock`) | Custom 72 MHz HSE+PLL config |
| Assembly | All inline in `port.c` | Same — inline in `port.c` |
| Startup code | Provided by PlatformIO | Existing `core.S` + `vector_table.S` |
| Linker script | Provided by PlatformIO | Existing `STM32F103C8T6.ld` |
| Handler naming | `_Handler` suffix (e.g. `SVC_Handler`) | `_handler` suffix (e.g. `SVC_handler`) — matches existing vector table |
| FPU support | PendSV has FPU lazy-stacking code | Removed — Cortex-M3 has no FPU |
| USART for debug | USART3 on Nucleo-F756ZG | Not used — HardFault blinks LED pattern |

## Memory Footprint

```
text     data     bss      total    target
1956     4        6320     8280     main.elf

Flash: ~1.96 KB / 64 KB  (3%)
RAM:   ~6.32 KB / 20 KB  (31%)
```

RAM breakdown:
- 5 task TCBs × ~1044 bytes each (256-word stack + metadata) = ~5220 bytes
- Kernel static variables (tick counter, critical section state, etc.)
- BSP variables

## Architecture

```
include/tinyrtos/kernel/
├── kernel.h        Kernel API (init, tick, sleep, critical sections)
├── task.h          TCB structure, task creation, stack management
├── scheduler.h     Priority + round-robin scheduler
├── port.h          Cortex-M3 hardware port API
├── fault.h         HardFault handler
├── semaphore.h     Counting/binary semaphore
├── mutex.h         Mutex (no priority inheritance)
└── queue.h         Ring-buffer message queue

src/kernel/
├── kernel.c        SysTick init, tick handler, sleep, critical sections
├── task.c          TCB array, stack watermark, exception frame build
├── scheduler.c     Priority scheduling with round-robin among equal priorities
├── port.c          PendSV/SVC/HardFault assembly, yield, exception config
├── fault.c         HardFault register capture + LED blink signal
├── semaphore.c     Semaphore take/give/give_from_isr
├── mutex.c         Mutex lock/unlock
└── queue.c         Queue send/receive/send_from_isr

src/bsp/blue_pill/
├── clock.c         72 MHz HSE+PLL configuration
├── clock.h
├── led.c           PC13 (onboard) + PA0 (external) LED driver
└── led.h
```

## Key Cortex-M3 Adaptations

### 1. No FPU (most important change)

The reference `PendSV_Handler` contains FPU lazy-stacking code for Cortex-M7:

```asm
; Reference (Cortex-M7) — NOT present in this port
tst lr, #0x10
vstmdbeq r0!, {s16-s31}  ; save FPU regs if frame is extended
```

This was removed entirely. Cortex-M3 has no FPU, and `vstmdb` would cause a UsageFault.

### 2. Handler Name Convention

The existing `vector_table.S` uses lowercase handler names:

```asm
.word SVC_handler          ; not SVC_Handler
.word pend_SV_handler      ; not PendSV_Handler
.word hard_fault_handler   ; not HardFault_Handler
.word SysTick_handler      ; matches reference
```

All C handler functions in `port.c` were renamed to match.

### 3. SysTick & SCB Registers

The original `stm32f103x8.h` was missing SysTick and had an incomplete SCB. Added:

```c
// SysTick (0xE000E010)
typedef struct {
    __IO uint32_t CTRL;
    __IO uint32_t LOAD;
    __IO uint32_t VAL;
    __I  uint32_t CALIB;
} SysTick_Type;

// Extended SCB (added CFSR, HFSR, DFSR, MMFAR, BFAR, AFSR)
// Added bit defines: SCB_ICSR_PENDSVSET_Msk, SCB_CCR_DIV_0_TRP_Msk, etc.
```

### 4. CMSIS Intrinsics

Since we don't link CMSIS, these inline functions were added to `stm32f103x8.h`:

```c
static inline void __disable_irq(void);
static inline void __enable_irq(void);
static inline void __DSB(void);
static inline void __ISB(void);
static inline uint32_t __get_PRIMASK(void);
static inline void __set_PRIMASK(uint32_t primask);
```

### 5. SystemCoreClock

The reference uses CMSIS's `SystemCoreClock`. We define it in `clock.c`:

```c
uint32_t SystemCoreClock = 8000000UL;  // default HSI
// After clock_init(): SystemCoreClock = 72000000UL
```

### 6. Clock Configuration (new)

Not present in the reference (PlatformIO handles it). Added `src/bsp/blue_pill/clock.c`:

- HSE (8 MHz crystal) → PLL ×9 = 72 MHz
- Flash latency: 2 wait states (required for >48 MHz)
- APB1 prescaler: /2 (APB1 max 36 MHz)
- APB2 prescaler: /1

### 7. HardFault Handler

The reference dumps registers over USART3. This port captures registers into global variables and blinks PC13 in a rapid pattern. Set a breakpoint on `fault_blink_forever()` to inspect the `fault_*` globals in a debugger.

## Kernel Configuration Constants

| Constant | Value | Location |
|----------|-------|----------|
| `RTOSK_MAX_TASKS` | 4 | `task.h` |
| `RTOSK_TASK_STACK_WORDS` | 256 (1024 bytes) | `task.h` |
| `RTOSK_TOTAL_TASK_SLOTS` | 5 (4 user + 1 idle) | `task.h` |
| SysTick period | 1 ms | `kernel.c` |
| PendSV priority | 0xFF (lowest) | `port.c` |
| SysTick priority | 0xFE | `port.c` |
| `__NVIC_PRIO_BITS` | 4 | `stm32f103x8.h` |

## Scheduling Algorithm

Priority-based preemptive scheduling with round-robin among equal priorities:

1. Every 1ms SysTick triggers `rtosk_kernel_tick()`
2. Tick handler wakes any sleeping tasks whose `wake_tick` has elapsed
3. Tick handler triggers PendSV (context switch)
4. PendSV saves current task state, calls `rtosk_scheduler_select_next()`
5. Scheduler scans from current task index (round-robin fairness)
6. Selects task with highest priority among READY tasks
7. Falls back to idle task (executes `wfi`) if no user task is ready
8. PendSV restores selected task state and returns to it

## Synchronization Primitives

### Semaphore (`rtosk_semaphore_t`)

- Binary or counting (depends on `initial_count`)
- Single-waiter: only one task can block on a given semaphore
- `take()` — decrement count or block
- `give()` — wake waiter or set count to 1
- `give_from_isr()` — ISR-safe variant (no critical section needed)

### Mutex (`rtosk_mutex_t`)

- Single-owner, single-waiter
- Re-entrant (same task can lock multiple times)
- No priority inversion protection (no priority inheritance)
- `lock()` — acquire or block until available
- `unlock()` — release and wake waiter if any

### Queue (`rtosk_queue_t`)

- Fixed-size ring buffer, caller provides storage
- Single-waiter on receive
- `send()` — enqueue item or return 0 if full (non-blocking)
- `receive()` — dequeue item or block until available
- `send_from_isr()` — ISR-safe variant

## Demo Application

Two tasks demonstrating preemptive scheduling:

```c
// Task 1 (priority 2): Toggle PC13 every 200ms
static void fast_blink_task(void) {
    for(;;) {
        bsp_led13_toggle();
        rtosk_kernel_sleep_ms(200);
    }
}

// Task 2 (priority 1): Toggle PA0 every 1000ms
static void slow_blink_task(void) {
    for(;;) {
        bsp_led0_toggle();
        rtosk_kernel_sleep_ms(1000);
    }
}
```

**Expected behavior:**
- PC13 (onboard LED) blinks at 5 Hz (200ms toggle)
- PA0 (external LED + 220Ω resistor to GND) blinks at 1 Hz (1000ms toggle)
- Both run independently — preemptive scheduling works

## Build & Flash

```bash
make clean && make
st-flash write main.bin 0x08000000
```

## Hardware Requirements

- Blue Pill board (STM32F103C8T6) with 8 MHz HSE crystal
- ST-Link or serial flasher
- External LED + 220Ω resistor on PA0 (optional, for second task output)
- PC13 onboard LED works out of the box
