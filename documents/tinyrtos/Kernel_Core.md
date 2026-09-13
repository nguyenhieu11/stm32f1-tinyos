# TinyRTOS Kernel Core — Porting Guide

Porting the kernel core from [stm32f1-tinyos-full-porting](https://github.com/nguyenhieu11/stm32f103c8t6-bare-metal) to a bare-metal STM32F103C8T6 project.

## What the Kernel Core Provides

| Feature | Type | Description |
|---------|------|-------------|
| SysTick initialization | Hardware setup | 1 kHz kernel tick from Cortex-M3 SysTick timer |
| Kernel startup | Boot | Switches MSP → PSP, launches first task via SVC |
| Task creation | API | Builds TCB + initial stack frame for new tasks |
| Cooperative yield | API | Triggers PendSV context switch |
| Sleep (sleep_ms) | API | Blocks task for N ms, yields CPU |
| Delay (delay_ms) | API | Busy-wait for N ms (no yield) |
| Tick counter | API | Read milliseconds since kernel start |
| CPU frequency | API | Read SystemCoreClock |
| Critical sections | API | Nested PRIMASK-based interrupt disable/enable |

## Dependency Graph

Features must be understood in this order — each one builds on what came before:

```
Phase 1: Hardware Foundation (independent building blocks)
┌──────────────────────────────────────────────────────────────┐
│  1. Device Header  ──→  2. Port Layer  ──→  3. Task Module  │
│  (stm32f103x8.h)       (port.c/h)           (task.c/h)      │
│                              │                    │          │
│                              └──→ 4. Scheduler ←─┘          │
│                                   (scheduler.c/h)            │
└──────────────────────────────────────────────────────────────┘

Phase 2: Kernel Features (each depends on earlier features)
┌──────────────────────────────────────────────────────────────────┐
│  5. Critical Sections ──┐                                        │
│  6. Tick Counter ───────┤                                        │
│  7. CPU Frequency ──────┤                                        │
│                         ▼                                        │
│  8. Kernel Tick Handler ──→ 9. SysTick Init ──→ 10. Delay (ms) │
│  11. Task Creation ─────────→ 12. Kernel Start                  │
│  13. Yield ─────────────────→ 14. Sleep (ms)                    │
└──────────────────────────────────────────────────────────────────┘
```

---

## Phase 1 — Hardware Foundation

These are independent building blocks that don't depend on each other.
They provide the hardware abstraction needed by the kernel.

---

### 1. Device Header Extensions

**File:** [`device_headers/stm32f103x8.h`](device_headers/stm32f103x8.h)

**Prerequisites:** None (this is the lowest layer)

The bare-metal project's device header was extended with definitions the RTOS needs.

#### 1a. SysTick Timer Registers

The Cortex-M3 SysTick timer is a 24-bit down-counter at `0xE000E010`.
Every Cortex-M3 has one — it's the standard way to generate an RTOS tick.

**What was added** ([stm32f103x8.h:316-328](device_headers/stm32f103x8.h#L316-L328)):

```c
typedef struct {
    __IO uint32_t CTRL;     /* Control: ENABLE, TICKINT, CLKSOURCE bits */
    __IO uint32_t LOAD;     /* Reload value (24-bit, counts down from here) */
    __IO uint32_t VAL;      /* Current value (write any value to reset) */
    __I  uint32_t CALIB;    /* Calibration (read-only, often unreliable) */
} SysTick_Type;

#define SysTick             ((SysTick_Type *) 0xE000E010UL)
#define SysTick_CTRL_ENABLE_Msk     (1UL << 0)   /* bit 0: start counter */
#define SysTick_CTRL_TICKINT_Msk    (1UL << 1)   /* bit 1: interrupt on underflow */
#define SysTick_CTRL_CLKSOURCE_Msk  (1UL << 2)   /* bit 2: 1=processor clock */
```

**Why it's needed:** `kernel.c` writes to `SysTick->LOAD`, `SysTick->VAL`, and `SysTick->CTRL` to configure the 1 ms tick.

#### 1b. SCB Extended for Fault Handling

**What is SCB?**

SCB = **System Control Block** — a block of memory-mapped registers at `0xE000ED00` that controls the Cortex-M3 processor's core functions. It's part of the ARM architecture itself (not STM32-specific — every Cortex-M3 has one). Think of it as the processor's "control panel."

**SCB register map** (at base `0xE000ED00`):

| Offset | Register | Purpose | Used by |
|--------|----------|---------|---------|
| 0x00 | CPUID | CPU identification (read-only) | — |
| 0x04 | ICSR | Interrupt Control & State | `port.c` — PendSV pending bit |
| 0x08 | VTOR | Vector Table Offset | — |
| 0x0C | AIRCR | System reset, priority grouping | — |
| 0x10 | SCR | Sleep mode config | — |
| 0x14 | CCR | Config Control | `port.c` — div-by-zero trap |
| 0x18 | SHP[12] | System Handler Priority | `port.c` — PendSV/SysTick priorities |
| 0x24 | SHCSR | System Handler Control & Status | — |
| 0x28 | CFSR | Configurable Fault Status | `fault.c` — read on HardFault |
| 0x2C | HFSR | Hard Fault Status | `fault.c` — read on HardFault |
| 0x30 | DFSR | Debug Fault Status | — |
| 0x34 | MMFAR | MemManage Fault Address | `fault.c` — read on HardFault |
| 0x38 | BFAR | Bus Fault Address | `fault.c` — read on HardFault |
| 0x3C | AFSR | Auxiliary Fault Status | — |

**What was added** ([stm32f103x8.h:274-294](device_headers/stm32f103x8.h#L274-L294)):

The bare-metal project already had a minimal `SCB_Type` with just `CPUID..SHCSR`. We extended it with the fault registers below `SHCSR`:

```c
typedef struct {
    /* ... existing registers (CPUID through SHCSR) ... */
    __IO uint32_t CFSR;    /* 0x28: Configurable Fault Status (MMFSR + BFSR + UFSR) */
    __IO uint32_t HFSR;    /* 0x2C: Hard Fault Status */
    __IO uint32_t DFSR;    /* 0x30: Debug Fault Status */
    __IO uint32_t MMFAR;   /* 0x34: MemManage Fault Address */
    __IO uint32_t BFAR;    /* 0x38: Bus Fault Address */
    __IO uint32_t AFSR;    /* 0x3C: Auxiliary Fault Status */
} SCB_Type;
```

And the bit definitions used by the port layer:

```c
#define SCB_ICSR_PENDSVSET_Msk   (1UL << 28)  /* Write 1 to trigger PendSV */
#define SCB_CCR_DIV_0_TRP_Msk    (1UL << 4)   /* Trap on divide-by-zero */
#define SCB_CCR_UNALIGN_TRP_Msk  (1UL << 3)   /* Trap on unaligned access */
```

**How we use SCB in this RTOS:**

| RTOS function | SCB register | What we do |
|---|---|---|
| `rtosk_port_yield()` | `SCB->ICSR` | Write bit 28 to trigger PendSV context switch |
| `rtosk_port_configure_exceptions()` | `SCB->SHP[]` | Set PendSV priority=0xFF, SysTick=0xFE |
| `rtosk_port_configure_faults()` | `SCB->CCR` | Enable divide-by-zero and unaligned-access traps |
| `rtosk_fault_hardfault_handler()` | `SCB->CFSR/HFSR/BFAR/MMFAR` | Read fault status for debugger inspection |

**Why it's needed:**
- `CFSR/HFSR/BFAR/MMFAR` → `fault.c` reads them on HardFault to diagnose what went wrong
- `SCB_ICSR_PENDSVSET_Msk` → `port.c` writes it to trigger PendSV (context switch)
- `SCB_CCR_DIV_0_TRP_Msk` → `port.c` enables divide-by-zero trap (otherwise ARM silently returns 0)

#### 1c. CMSIS Intrinsics

Inline functions that wrap ARM assembly instructions for portable C code.

**What was added** ([stm32f103x8.h:353-400](device_headers/stm32f103x8.h#L353-L400)):

| Function | Assembly | Purpose |
|----------|----------|---------|
| `__disable_irq()` | `cpsid i` | Disable all interrupts (set PRIMASK) |
| `__enable_irq()` | `cpsie i` | Re-enable interrupts (clear PRIMASK) |
| `__DSB()` | `dsb 0xF` | Data sync barrier (wait for memory writes) |
| `__ISB()` | `isb 0xF` | Instruction sync barrier (flush pipeline) |
| `__get_PRIMASK()` | `MRS primask` | Read interrupt enable state |

**Why they're needed:** The port layer uses these for interrupt control (`irq_save/restore`) and memory barriers after writing to SCB registers.

#### 1d. SystemCoreClock Variable

**What was added** ([stm32f103x8.h:409](device_headers/stm32f103x8.h#L409)):

```c
extern uint32_t SystemCoreClock;
```

**Defined in** [`main.c:28`](main.c#L28): `uint32_t SystemCoreClock = 8000000UL;`

**Why it's needed:** The SysTick reload value is computed as `SystemCoreClock / 1000 - 1`. At 8 MHz HSI → LOAD = 7999 (1.000 ms). At 72 MHz PLL → LOAD = 71999.

---

### 2. Port Layer (Cortex-M3 Hardware Interface)

**Files:**
- [`include/tinyrtos/kernel/port.h`](include/tinyrtos/kernel/port.h) — API declarations
- [`src/kernel/port.c`](src/kernel/port.c) — Implementation

**Prerequisites:** [1. Device Header](#1-device-header-extensions)

The port layer is the **ONLY** part of the RTOS that contains architecture-specific code (inline assembly, naked functions). Everything above it is portable C.

#### 2a. Constants

[port.h:15-18](include/tinyrtos/kernel/port.h#L15-L18):

```c
#define RTOSK_XPSR_T_BIT    0x01000000UL   /* Thumb bit for xPSR */
#define RTOSK_EXC_RETURN_PSP 0xFFFFFFFDUL  /* Return to Thread mode using PSP */
```

To understand these constants, we need four Cortex-M3 concepts:

##### PSR (Program Status Register)

A 32-bit register inside the CPU that stores the processor's current status flags:

```
Bit 31  30  29  28  27     24    16         8          0
  ┌───┬───┬───┬───┬───┬─────┬─────┬─────────┬──────────┐
  │ N │ Z │ C │ V │ Q │ ... │ T   │ ...     │ ISR_NUM  │
  └───┴───┴───┴───┴───┴─────┴─────┴─────────┴──────────┘
    │   │   │   │   │         │                 │
    │   │   │   │   │         │                 └─ Which interrupt/exception
    │   │   │   │   │         │                    is active (0 = none)
    │   │   │   │   │         └─ Thumb bit (MUST be 1 on Cortex-M3)
    │   │   │   │   └─ Overflow/saturation
    │   │   │   └─ Overflow flag
    │   │   └─ Carry flag
    │   └─ Zero flag
    └─ Negative flag
```

The N, Z, C, V flags are set by arithmetic instructions (`ADDS`, `SUBS`, `CMP`).
That's how `if (a > b)` works — the CPU does `CMP a, b`, sets flags in PSR, then the branch instruction reads PSR to decide.

##### MSP and PSP (Two Stack Pointers)

Cortex-M3 has **two** stack pointers (unlike most CPUs which have one):

```
┌─────────────────────────────────────────────┐
│  MSP (Main Stack Pointer)                   │
│  - Used by: ISRs, exception handlers, boot  │
│  - Points to: _estack (0x20005000)          │
│  - Shared by all interrupts                 │
├─────────────────────────────────────────────┤
│  PSP (Process Stack Pointer)                │
│  - Used by: task code (Thread mode)         │
│  - Points to: current task's private stack  │
│  - Each task has its own PSP value          │
└─────────────────────────────────────────────┘
```

**Why two?** So each task gets its own stack, and ISRs don't corrupt task stacks:

```
  _estack (0x20005000)
  ┌──────────────────┐
  │                  │
  │  ISR stack (MSP) │ ← All ISRs share this (grows down)
  │                  │
  ├──────────────────┤
  │                  │
  │  Task A stack    │ ← PSP when Task A runs (1 KB each)
  ├──────────────────┤
  │                  │
  │  Task B stack    │ ← PSP when Task B runs
  ├──────────────────┤
  │  ...             │
  └──────────────────┘
```

When an ISR fires, the CPU **automatically** switches to MSP. When it returns, it switches back to PSP. Task stacks are never touched by ISRs.

##### Thread Mode vs Handler Mode

The CPU operates in one of two modes:

```
┌─────────────────────────────────────────────────────────┐
│  Handler Mode                                           │
│  - Active during: ISRs, exceptions (SVC, PendSV, etc.) │
│  - Stack: always MSP                                    │
│  - Privilege: always privileged (full access)           │
├─────────────────────────────────────────────────────────┤
│  Thread Mode                                            │
│  - Active during: normal task code                      │
│  - Stack: MSP or PSP (configurable via CONTROL reg)     │
│  - Privilege: privileged or unprivileged (configurable) │
└─────────────────────────────────────────────────────────┘
```

How they work together in this RTOS:

```
Time ─────────────────────────────────────────────────────→

  Thread mode (PSP)        Handler mode (MSP)       Thread mode (PSP)
  Task A running...      │  SysTick fires!         │  Task B running...
                         │  CPU switches to MSP    │  CPU switches back
  ┌───────────────┐      │  ┌───────────────┐      │  ┌───────────────┐
  │ Use PSP       │─────→│  │ Use MSP       │─────→│  │ Use PSP       │
  │ (Task A stack)│      │  │ (ISR stack)   │      │  │ (Task B stack)│
  └───────────────┘      │  └───────────────┘      │  └───────────────┘
                         │  SysTick handler runs   │  (context switch
                         │  → kernel_tick()        │   happened in
                         │  → PendSV fires         │   PendSV handler)
                         │  → save A, restore B    │
                         │  → exception return     │
```

##### xPSR_T_BIT (0x01000000)

xPSR bit 24 = "T" (Thumb) bit. Cortex-M3 **only** supports Thumb instructions. If T=0, the CPU faults immediately.

When the CPU pushes an exception frame (on SVC, PendSV, SysTick), it **automatically** sets T=1 in the saved xPSR. So real tasks always have T=1.

But when we **build a fake frame** for a new task (in `task_build_stack`), there's no real exception entry — we manually write the frame. If we set xPSR to 0, the first `bx lr` pops xPSR=0 → T=0 → **HardFault**.

So we must write `0x01000000` (bit 24 = 1) to fake what the hardware would have done:

```c
*(--sp) = RTOSK_XPSR_T_BIT;   // xPSR: bit 24 = 1 (Thumb mode)
```

##### EXC_RETURN_PSP (0xFFFFFFFD)

When the CPU enters an exception, it automatically loads LR with a special **EXC_RETURN** value. This is NOT a real address — it's a code that tells the CPU how to return:

```
LR value       Meaning
─────────────────────────────────────────────────
0xFFFFFFF9     Return to Thread mode, use MSP
0xFFFFFFFD     Return to Thread mode, use PSP   ← This RTOS uses this
0xFFFFFFF1     Return to Handler mode, use MSP
```

The CPU detects EXC_RETURN by checking bits [31:4] = `0xFFFFFFF`. When you do `bx lr` with one of these values, the CPU does an **exception return** (pops the exception frame from the stack) instead of a normal branch.

**In our SVC_handler** ([port.c:30-43](src/kernel/port.c#L30-L43)):

```asm
SVC_handler:                     // We're in Handler mode (using MSP)
    ...
    msr psp, r0                  // Set PSP = task's stack
    mrs r0, control              // Read CONTROL register
    orr r0, r0, #2               // Set bit 1 = "use PSP in Thread mode"
    msr control, r0              // Write CONTROL
    ldr lr, =0xFFFFFFFD          // ← EXC_RETURN: "return to Thread mode using PSP"
    bx lr                        // CPU sees 0xFFFFFFFD → exception return via PSP
```

When SVC fires, the CPU already set LR = `0xFFFFFFF9` (return using MSP). But we want the task to run on PSP (each task has its own stack). So we overwrite LR with `0xFFFFFFFD` before `bx lr`. The CPU then pops R0-R3, R12, LR, PC, xPSR from PSP and starts executing the task function.

#### 2b. Interrupt Control

[port.c:89-101](src/kernel/port.c#L89-L101):

```c
uint32_t rtosk_port_irq_save(void) {
    uint32_t primask = __get_PRIMASK();   // Save current state
    __disable_irq();                       // Disable interrupts
    __DSB(); __ISB();                      // Barriers (ensure effect is immediate)
    return primask;                        // Return old state (for later restore)
}

void rtosk_port_irq_restore(uint32_t primask) {
    if ((primask & 1UL) == 0UL) {         // Only re-enable if they WERE enabled
        __enable_irq();
    }
}
```

**Why barriers matter:** After `cpsid i`, the CPU pipeline may have already fetched the next few instructions with interrupts enabled. `DSB` ensures all pending memory operations complete. `ISB` flushes the pipeline so subsequent instructions execute with interrupts truly disabled.

#### 2c. PendSV Trigger (Context Switch Request)

[port.c:82-87](src/kernel/port.c#L82-L87):

```c
void rtosk_port_yield(void) {
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;   // Set PendSV pending bit
    __DSB();                                // Ensure the write completes
    __ISB();                                // Flush pipeline
}
```

**How it works:** Writing to bit 28 of SCB->ICSR makes the PendSV exception pending. Since PendSV has the LOWEST priority (0xFF), it doesn't fire immediately — it waits until the current ISR (e.g., SysTick) returns. This ensures clean ordering: ISRs run first, then the context switch happens.

#### 2d. Exception Priority Configuration

[port.c:102-108](src/kernel/port.c#L102-L108):

```c
void rtosk_port_configure_exceptions(void) {
    NVIC_SetPriority(PendSV_IRQn, 0xFFU);    // Lowest priority
    NVIC_SetPriority(SysTick_IRQn, 0xFEU);   // One above PendSV
}
```

**Priority order** (lower number = higher priority):

```
0x00 ─── Highest (most peripheral ISRs)
 ...
0x03 ─── EXTI, UART, etc.
 ...
0xFE ─── SysTick (kernel tick — wakes tasks, sets PendSV)
0xFF ─── PendSV (context switch — runs last)
```

**Why this ordering:** SysTick must run BEFORE PendSV so it can wake blocked tasks before the scheduler picks the next task.

#### 2e. Fault Configuration

[port.c:109-113](src/kernel/port.c#L109-L113):

```c
void rtosk_port_configure_faults(void) {
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;      // Trap divide-by-zero
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;    // Trap unaligned access
}
```

Without these, divide-by-zero silently returns 0 and unaligned access may produce wrong results.

#### 2f. SVC Handler — Launch First Task

[port.c:30-43](src/kernel/port.c#L30-L43) — `__attribute__((naked))`:

```asm
SVC_handler:
    ldr r0, =rtosk_task_get_stack_pointer  // Get first task's SP
    blx r0                                 // (C call, returns SP in r0)
    ldmia r0!, {r4-r11}                    // Restore R4-R11 from task stack
    msr psp, r0                            // PSP = task stack (past R4-R11)
    mrs r0, control                        // Read CONTROL register
    orr r0, r0, #2                         // Set bit 1 (use PSP in Thread mode)
    msr control, r0                        // Write CONTROL → now using PSP
    isb                                    // Flush pipeline
    ldr lr, =0xFFFFFFFD                    // EXC_RETURN: Thread mode + PSP
    bx lr                                  // Exception return → task starts!
```

**What happens step by step:**

1. Get the first task's saved stack pointer (points to R4 in the initial frame)
2. Restore R4-R11 from the stack (8 words, all zeros for a new task)
3. Advance PSP past R4-R11 (now points to the hardware exception frame)
4. Switch CONTROL bit 1 so Thread mode uses PSP instead of MSP
5. Set LR to EXC_RETURN value (0xFFFFFFFD = Thread mode + PSP)
6. `bx lr` triggers exception return → hardware pops R0-R3, R12, LR, PC, xPSR → task starts executing at its PC (the task function)

**Why `naked`:** The compiler must not add prologue/epilogue code (push/pop, stack frame). The function is pure assembly.

#### 2g. PendSV Handler — Context Switch

[port.c:51-67](src/kernel/port.c#L51-L67) — `__attribute__((naked))`:

```asm
pend_SV_handler:
    mrs r0, psp                             // 1. Get current task's PSP
    stmdb r0!, {r4-r11}                     // 2. Save R4-R11 to task stack
    push {r4, lr}                           // 3. Save registers we'll clobber
    bl  rtosk_task_save_stack_pointer       // 4. Store SP in current TCB
    bl  rtosk_scheduler_select_next         // 5. Pick next task
    bl  rtosk_task_get_stack_pointer        // 6. Get next task's SP
    pop {r4, lr}                            // 7. Restore clobbered registers
    ldmia r0!, {r4-r11}                     // 8. Restore R4-R11 from new stack
    msr psp, r0                             // 9. Set PSP to new task's stack
    bx lr                                   // 10. Hardware pops R0-R3, R12,
                                            //     LR, PC, xPSR → new task runs
```

**Context switch flow:**

```
                    Current Task
                         │
    ┌────────────────────▼────────────────────┐
    │ 1. Read PSP (current task's stack ptr)   │
    │ 2. Push R4-R11 onto task stack (save)    │
    │ 4. Save SP into current task's TCB       │
    │ 5. Scheduler picks next task             │
    │ 6. Get next task's saved SP from TCB     │
    │ 8. Pop R4-R11 from new task's stack      │
    │ 9. Set PSP = new task's stack            │
    └────────────────────┬────────────────────┘
                         │
                    New Task (hardware pops R0-R3, R12, LR, PC, xPSR)
```

**Why only R4-R11:** The Cortex-M3 hardware automatically saves/restores R0-R3, R12, LR, PC, xPSR on exception entry/exit. We only need to save the registers the hardware doesn't handle.

#### 2h. SysTick Handler — Kernel Tick Entry Point

[port.c:20-22](src/kernel/port.c#L20-L22):

```c
void SysTick_handler(void) {
    rtosk_kernel_tick();
}
```

**Naming:** The function name `SysTick_handler` (lowercase `h`) must match the vector table entry exactly ([vector_table.S:49](vector_table.S#L49): `.word SysTick_handler`). On Linux, the toolchain is case-sensitive.

#### 2i. HardFault Handler

[port.c:72-81](src/kernel/port.c#L72-L81):

```asm
hard_fault_handler:
    tst lr, #4          // Test EXC_RETURN bit 2: which stack was used?
    ite eq              // If-Then-Else
    mrseq r0, msp       // bit 2 = 0 → MSP was used (handler mode)
    mrsne r0, psp       // bit 2 = 1 → PSP was used (thread mode)
    mov r1, lr          // Pass EXC_RETURN as second argument
    b rtosk_fault_hardfault_handler  // Branch to C handler (never returns)
```

**Why test bit 2 of LR:** During a fault, LR contains EXC_RETURN. Bit 2 tells us whether the fault happened in Thread mode (PSP) or Handler mode (MSP). We pass the correct stack pointer so the C handler can read the stacked registers.

#### 2j. Stack Alignment

[port.c:16-18](src/kernel/port.c#L16-L18):

```c
uint32_t * rtosk_port_align_stack_pointer(uint32_t *sp) {
    return (uint32_t *)((uint32_t)sp & ~0x7UL);   // Clear bottom 3 bits
}
```

**Why:** The AAPCS (ARM calling convention) requires the stack pointer to be 8-byte aligned at function calls. When building a new task's initial stack frame, the top of the stack may not be naturally aligned.

---

### 3. Task Module (TCB Management)

**Files:**
- [`include/tinyrtos/kernel/task.h`](include/tinyrtos/kernel/task.h) — TCB struct, states, API
- [`src/kernel/task.c`](src/kernel/task.c) — Implementation

**Prerequisites:** [2. Port Layer](#2-port-layer-cortex-m3-hardware-interface) (for `RTOSK_XPSR_T_BIT`, `rtosk_port_align_stack_pointer`)

#### 3a. Task Control Block (TCB)

[task.h:54-61](include/tinyrtos/kernel/task.h#L54-L61):

```c
typedef struct {
    uint32_t * sp;                          // Saved stack pointer
    uint32_t stack[RTOSK_TASK_STACK_WORDS]; // Private stack (256 words = 1 KB)
    rtosk_task_state_t state;               // READY, BLOCKED, IDLE, etc.
    uint32_t wake_tick;                     // Wake at this tick (for sleep)
    uint32_t priority;                      // Higher = more important
    const char * name;                      // For debugging
} rtosk_task_t;
```

**Key design decisions:**
- Stack is **inside** the TCB (no malloc — deterministic memory usage)
- `sp` points INTO the `stack[]` array (updated on every context switch)
- 5 TCBs × 1 KB = 5 KB RAM for task stacks

#### 3b. Task States

[task.h:33-41](include/tinyrtos/kernel/task.h#L33-L41):

```c
typedef enum {
    RTOSK_TASK_READY = 0U,              // Ready to run
    RTOSK_TASK_BLOCKED,                 // Sleeping (waiting for wake_tick)
    RTOSK_TASK_BLOCKED_ON_SEMAPHORE,    // Waiting on semaphore
    RTOSK_TASK_BLOCKED_ON_QUEUE,        // Waiting on queue
    RTOSK_TASK_BLOCKED_ON_MUTEX,        // Waiting on mutex
    RTOSK_TASK_IDLE                     // Idle task (runs WFI)
} rtosk_task_state_t;
```

#### 3c. Stack Watermark

[task.h:51](include/tinyrtos/kernel/task.h#L51): `#define RTOSK_STACK_WATERMARK 0xDEADBEEFUL`

On task creation, every stack word is filled with `0xDEADBEEF`. By scanning from the bottom, we count how many watermark words remain — this tells us the high-water mark (maximum stack usage).

#### 3d. Initial Stack Frame Construction

[task.c:54-71](src/kernel/task.c#L54-L71):

```c
static uint32_t * rtosk_task_build_stack(rtosk_task_t * task, rtosk_task_func_t task_func) {
    uint32_t * sp = &task->stack[RTOSK_TASK_STACK_WORDS];  // Top of stack
    sp = rtosk_port_align_stack_pointer(sp);                // 8-byte align

    // Hardware exception frame (CPU pushes this on exception entry)
    *(--sp) = RTOSK_XPSR_T_BIT;          // xPSR: Thumb bit set (bit 24)
    *(--sp) = (uint32_t)task_func;        // PC:   task entry function
    *(--sp) = (uint32_t)rtosk_task_exit_trap; // LR: safety trap if task returns
    *(--sp) = 0x00000000UL;              // R12
    *(--sp) = 0x00000000UL;              // R3
    *(--sp) = 0x00000000UL;              // R2
    *(--sp) = 0x00000000UL;              // R1
    *(--sp) = 0x00000000UL;              // R0

    // Software-saved frame (PendSV pushes this on context switch)
    *(--sp) = 0x00000000UL;              // R11
    *(--sp) = 0x00000000UL;              // R10
    *(--sp) = 0x00000000UL;              // R9
    *(--sp) = 0x00000000UL;              // R8
    *(--sp) = 0x00000000UL;              // R7
    *(--sp) = 0x00000000UL;              // R6
    *(--sp) = 0x00000000UL;              // R5
    *(--sp) = 0x00000000UL;              // R4
    return sp;                           // SP now points to R4
}
```

**Stack layout (high address at top):**

```
stack[255] ┌─────────────┐ ← Top of stack
           │  (padding)  │ ← Alignment to 8-byte boundary
           ├─────────────┤
           │  xPSR       │ ← 0x01000000 (Thumb bit)
           │  PC         │ ← task_func pointer
           │  LR         │ ← rtosk_task_exit_trap
           │  R12        │ ← 0
           │  R3         │ ← 0
           │  R2         │ ← 0
           │  R1         │ ← 0
           │  R0         │ ← 0
           ├─────────────┤ ← Hardware-saved frame (8 words)
           │  R11        │ ← 0
           │  ...        │
           │  R4         │ ← 0
           ├─────────────┤ ← Software-saved frame (8 words)
           │             │ ← task->sp points here
stack[0]   └─────────────┘
```

When PendSV "restores" this context, it loads R4-R11 (zeros), advances SP, and the hardware pops R0-R3, R12, LR, PC, xPSR — the task starts executing at `task_func`.

#### 3e. Task Creation

[task.c:90-96](src/kernel/task.c#L90-L96):

```c
void rtosk_task_create(rtosk_task_func_t task_func, uint32_t priority, const char * name) {
    if (task_func == 0) return;                     // Null check
    if (RTOSK_TASK_COUNT >= RTOSK_MAX_TASKS) return; // Slot check
    rtosk_task_t * task = &RTOSK_TASKS[RTOSK_TASK_COUNT];
    rtosk_task_watermark_stack(task);                // Fill with 0xDEADBEEF
    task->sp = rtosk_task_build_stack(task, task_func); // Build initial frame
    task->state = RTOSK_TASK_READY;                  // Ready to run
    task->wake_tick = 0UL;
    task->priority = priority;
    task->name = name;
    RTOSK_TASK_COUNT++;
}
```

#### 3f. Task Blocking and Waking

[task.c:112-116](src/kernel/task.c#L112-L116) — Block:
```c
void rtosk_task_block_current_until(uint32_t wake_tick) {
    if (RTOSK_CURRENT_TASK == RTOSK_IDLE_TASK_INDEX) return;
    RTOSK_TASKS[RTOSK_CURRENT_TASK].state = RTOSK_TASK_BLOCKED;
    RTOSK_TASKS[RTOSK_CURRENT_TASK].wake_tick = wake_tick;
}
```

[task.c:118-130](src/kernel/task.c#L118-L130) — Wake:
```c
uint32_t rtosk_task_update_blocked(uint32_t current_tick) {
    for (uint32_t i = 0UL; i < RTOSK_TASK_COUNT; i++) {
        if (RTOSK_TASKS[i].state == RTOSK_TASK_BLOCKED) {
            if ((int32_t)(current_tick - RTOSK_TASKS[i].wake_tick) >= 0) {
                RTOSK_TASKS[i].state = RTOSK_TASK_READY;  // Wake up!
            }
        }
    }
}
```

**Signed comparison trick:** `(int32_t)(current_tick - wake_tick) >= 0` correctly handles uint32 wraparound. If current = 0x10 and wake = 0xFFFFFFF0, the unsigned subtraction gives 0x20 (positive) — the task wakes correctly.

---

### 4. Scheduler

**Files:**
- [`include/tinyrtos/kernel/scheduler.h`](include/tinyrtos/kernel/scheduler.h)
- [`src/kernel/scheduler.c`](src/kernel/scheduler.c)

**Prerequisites:** [3. Task Module](#3-task-module-tcb-management)

#### Priority-Based with Round-Robin

[scheduler.c:9-42](src/kernel/scheduler.c#L9-L42):

```c
void rtosk_scheduler_select_next(void) {
    // Scan from current task for round-robin fairness
    for (uint32_t offset = 0UL; offset < task_count; offset++) {
        uint32_t index = (current + offset) % task_count;
        rtosk_task_t * task = rtosk_task_get(index);
        if (task->state != RTOSK_TASK_READY) continue;
        if (found_ready == 0UL || task->priority > best_priority) {
            best_index = index;
            best_priority = task->priority;
            found_ready = 1UL;
        }
    }
    // Fall back to idle task if no user task is READY
}
```

**How round-robin works:** The scan starts from the current task's index, not from 0. If tasks A, B, C all have priority 2:
- A running → next scan finds B first → B runs
- B running → next scan finds C first → C runs
- C running → next scan wraps, finds A first → A runs

Higher-priority tasks always win. Round-robin only applies among equal priorities.

---

## Phase 2 — Kernel Core Features

These are the public API functions in `kernel.c`. Each one depends on the Phase 1 foundations and on earlier Phase 2 features.

---

### 5. Nested Critical Sections

**Depends on:** [2. Port Layer](#2b-interrupt-control) (`rtosk_port_irq_save`, `rtosk_port_irq_restore`)

**Header:** [kernel.h:351-362](include/tinyrtos/kernel/kernel.h#L351-L362)
**Source:** [kernel.c:478-512](src/kernel/kernel.c#L478-L512)

```c
void rtosk_kernel_enter_critical(void) {
    uint32_t primask = rtosk_port_irq_save();   // Save + disable IRQ
    if (RTOSK_CRITICAL_NESTING == 0UL) {
        RTOSK_CRITICAL_PRIMASK = primask;        // Save original state (first entry only)
    }
    RTOSK_CRITICAL_NESTING++;
}

void rtosk_kernel_exit_critical(void) {
    if (RTOSK_CRITICAL_NESTING == 0UL) return;   // Underflow guard
    RTOSK_CRITICAL_NESTING--;
    if (RTOSK_CRITICAL_NESTING == 0UL) {
        rtosk_port_irq_restore(RTOSK_CRITICAL_PRIMASK);  // Restore original state
    }
}
```

**Why nested?** `sleep_ms()` calls `enter_critical()` internally. If the caller is already in a critical section, the inner `exit_critical()` must NOT re-enable interrupts.

**Static state** ([kernel.c:94-108](src/kernel/kernel.c#L94-L108)):

```c
static uint32_t RTOSK_CRITICAL_NESTING = 0UL;  // How deep are we?
static uint32_t RTOSK_CRITICAL_PRIMASK  = 0UL;  // Original PRIMASK (from first entry)
```

---

### 6. Tick Counter Access

**Depends on:** Nothing (just a variable)

**Header:** [kernel.h:305-308](include/tinyrtos/kernel/kernel.h#L305-L308)
**Source:** [kernel.c:64](src/kernel/kernel.c#L64), [kernel.c:358-360](src/kernel/kernel.c#L358-L360)

```c
static volatile uint32_t RTOSK_SYSTICKS = 0UL;  // kernel.c:64

uint32_t rtosk_kernel_get_ticks(void) {          // kernel.c:358
    return RTOSK_SYSTICKS;
}
```

**Why `volatile`:** Modified in SysTick ISR, read from task code. Without volatile, the compiler might optimize away re-reads from memory.

---

### 7. CPU Frequency Query

**Depends on:** [1d. SystemCoreClock](#1d-systemcoreclock-variable)

**Header:** [kernel.h:319-322](include/tinyrtos/kernel/kernel.h#L319-L322)
**Source:** [kernel.c:365-367](src/kernel/kernel.c#L365-L367)

```c
uint32_t rtosk_get_cpu_freq(void) {
    return SystemCoreClock;
}
```

---

### 8. Kernel Tick Handler

**Depends on:** [6. Tick Counter](#6-tick-counter-access), [3. Task Module](#3f-task-blocking-and-waking) (`update_blocked`), [2c. PendSV Trigger](#2c-pendsv-trigger-context-switch-request)

**Source:** [kernel.c:538-549](src/kernel/kernel.c#L538-L549)

```c
void rtosk_kernel_tick(void) {
    RTOSK_SYSTICKS++;                              // 1. Increment tick counter
    if (RTOSK_KERNEL_INIT == 0U) return;           // 2. Before kernel start: just count
    rtosk_task_update_blocked(RTOSK_SYSTICKS);     // 3. Wake tasks whose sleep expired
    rtosk_port_yield();                            // 4. Trigger PendSV (context switch)
}
```

**Called from:** `SysTick_handler` in [port.c:20-22](src/kernel/port.c#L20-L22)

**Execution flow every 1 ms:**

```
Hardware: SysTick counter reaches 0
    ↓
CPU: Enters SysTick_handler (ISR, uses MSP)
    ↓
kernel_tick():
    ├── RTOSK_SYSTICKS++ (tick 1000 → 1001)
    ├── update_blocked() → task with wake_tick=1001 becomes READY
    └── port_yield() → SCB->ICSR = PENDSVSET, DSB, ISB
    ↓
CPU: SysTick returns → PendSV fires (lower priority)
    ↓
pend_SV_handler: context switch to the newly-READY task
```

---

### 9. SysTick Initialization

**Depends on:** [1a. SysTick Registers](#1a-systick-timer-registers), [2d. Exception Priorities](#2d-exception-priority-configuration), [8. Kernel Tick Handler](#8-kernel-tick-handler)

**Header:** [kernel.h:115](include/tinyrtos/kernel/kernel.h#L115)
**Source:** [kernel.c:165-227](src/kernel/kernel.c#L165-L227)

```c
void rtosk_kernel_systick_init(void) {
    if (RTOSK_KERNEL_SYSTICK_INIT) return;         // Idempotent guard

    // 1. Set PendSV=0xFF (lowest), SysTick=0xFE (one above)
    rtosk_port_configure_exceptions();

    // 2. Reload value for 1 ms tick
    //    At 8 MHz: LOAD = 8000000/1000 - 1 = 7999
    SysTick->LOAD = (SystemCoreClock / 1000UL) - 1UL;

    // 3. Clear current counter (reset to LOAD immediately)
    SysTick->VAL = 0UL;

    // 4. Start SysTick: processor clock + interrupt + enable
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk   |
                    SysTick_CTRL_ENABLE_Msk;

    RTOSK_KERNEL_SYSTICK_INIT = 1U;
}
```

**Register details** (see [1a](#1a-systick-timer-registers)):
- `LOAD`: Counter counts from LOAD down to 0, then reloads. LOAD+1 cycles = 1 ms.
- `VAL`: Writing any value resets the counter to LOAD (clean start).
- `CTRL`: CLKSOURCE=1 (processor clock), TICKINT=1 (interrupt on underflow), ENABLE=1 (start).

---

### 10. Busy-wait Delay

**Depends on:** [6. Tick Counter](#6-tick-counter-access)

**Header:** [kernel.h:231-240](include/tinyrtos/kernel/kernel.h#L231-L240)
**Source:** [kernel.c:369-373](src/kernel/kernel.c#L369-L373)

```c
void rtosk_kernel_delay_ms(uint32_t ms) {
    uint32_t start = RTOSK_SYSTICKS;
    while ((RTOSK_SYSTICKS - start) < ms)   // Unsigned subtraction handles wraparound
        ;                                    // Spin (no yield)
}
```

**When to use:** Short delays before kernel starts, or in ISRs where sleeping is forbidden. For everything else, prefer `sleep_ms()`.

---

### 11. Task Creation Wrapper

**Depends on:** [3e. Task Creation](#3e-task-creation)

**Header:** [kernel.h:202](include/tinyrtos/kernel/kernel.h#L202)
**Source:** [kernel.c:336-339](src/kernel/kernel.c#L336-L339)

```c
void rtosk_kernel_create_task(rtosk_task_func_t task_func, uint32_t priority, const char * name) {
    rtosk_task_create(task_func, priority, name);   // Thin wrapper
}
```

**Why a wrapper?** Keeps the public API in the `rtosk_kernel_*` namespace while the implementation details are in `rtosk_task_*`.

---

### 12. Kernel Startup

**Depends on:** [11. Task Creation](#11-task-creation-wrapper), [2f. SVC Handler](#2f-svc-handler--launch-first-task), [2e. Fault Config](#2e-fault-configuration)

**Header:** [kernel.h:152-167](include/tinyrtos/kernel/kernel.h#L152-L167)
**Source:** [kernel.c:250-330](src/kernel/kernel.c#L250-L330)

```c
void rtosk_kernel_start(void) {
    if (RTOSK_KERNEL_INIT) return;

    // 1. Sanity check: at least one task must exist
    if (rtosk_task_get_count() == 0UL) { for (;;) {} }

    // 2. Enable divide-by-zero and unaligned-access traps
    rtosk_port_configure_faults();

    // 3. Create idle task (WFI loop, priority 0)
    rtosk_task_init_idle();

    RTOSK_KERNEL_INIT = 1U;

    // 4. Launch first task — NEVER RETURNS
    rtosk_port_start_first_task();    // → svc #0 → SVC_handler
}
```

**Boot sequence:**

```
reset_handler (core.S)
    │  Set MSP, copy .data, zero .bss
    ▼
main() (main.c)
    │  led_init(), create tasks, systick_init()
    ▼
rtosk_kernel_start() (kernel.c)
    │  configure_faults(), init_idle()
    ▼
svc #0 → SVC_handler (port.c)
    │  Restore first task's R4-R11
    │  Set PSP = task stack
    │  Switch CONTROL to PSP
    │  bx lr (EXC_RETURN)
    ▼
First task begins executing on PSP stack
    │  MSP is now used only by ISRs
    ▼
Every 1ms: SysTick → kernel_tick → PendSV → context switch
```

---

### 13. Cooperative Yield

**Depends on:** [2c. PendSV Trigger](#2c-pendsv-trigger-context-switch-request)

**Header:** [kernel.h:308](include/tinyrtos/kernel/kernel.h#L308)
**Source:** [kernel.c:442-445](src/kernel/kernel.c#L442-L445)

```c
void rtosk_kernel_yield(void) {
    rtosk_port_yield();     // → SCB->ICSR = PENDSVSET, DSB, ISB
}
```

If no other task is READY, the scheduler picks the same task (or idle) and execution resumes immediately.

---

### 14. Cooperative Sleep (Primary Timing Mechanism)

**Depends on:** [5. Critical Sections](#5-nested-critical-sections), [3f. Task Blocking](#3f-task-blocking-and-waking), [13. Yield](#13-cooperative-yield), [6. Tick Counter](#6-tick-counter-access)

**Header:** [kernel.h:267-288](include/tinyrtos/kernel/kernel.h#L267-L288)
**Source:** [kernel.c:403-413](src/kernel/kernel.c#L403-L413)

```c
void rtosk_kernel_sleep_ms(uint32_t ms) {
    rtosk_kernel_enter_critical();                 // 1. Disable IRQ (prevent race)
    uint32_t wake_tick = RTOSK_SYSTICKS + ms;     // 2. Compute wake time
    rtosk_task_block_current_until(wake_tick);     // 3. Block task
    rtosk_kernel_exit_critical();                  // 4. Restore IRQ
    rtosk_kernel_yield();                          // 5. Context switch away
}
```

**What happens when a task calls `sleep_ms(100)` at tick 500:**

```
sleep_ms(100):
    enter_critical()       → IRQ disabled
    wake_tick = 500 + 100 = 600
    block_until(600)       → state = BLOCKED, wake_tick = 600
    exit_critical()        → IRQ re-enabled
    yield()                → PendSV fires
        │
        ▼ PendSV saves this task, scheduler picks another
        │
   ... 100 ms pass (SysTick increments counter each ms) ...
        │
   tick 600: SysTick → kernel_tick() → update_blocked()
        │  task's wake_tick (600) reached → state = READY
        │
   next PendSV: scheduler picks this task
        │
        ▼
    sleep_ms() returns     → task continues where it left off
```

**Why enter_critical?** Without it, a SysTick could fire between reading `RTOSK_SYSTICKS` and blocking the task, causing the task to use a stale tick value and sleep 1ms longer than intended.

---

## Files Changed

| File | Status | What was added |
|------|--------|---------------|
| `device_headers/stm32f103x8.h` | Modified | SysTick_Type, SCB fault regs, CMSIS intrinsics, SystemCoreClock |
| `include/tinyrtos/kernel/kernel.h` | **New** | Kernel core API (364 lines, extensively commented) |
| `src/kernel/kernel.c` | **New** | Kernel core implementation (549 lines, extensively commented) |
| `include/tinyrtos/kernel/task.h` | **New** | TCB struct, task states, task API |
| `src/kernel/task.c` | **New** | TCB array, stack building, task lifecycle |
| `include/tinyrtos/kernel/port.h` | **New** | Port layer API, constants |
| `src/kernel/port.c` | **New** | SVC/PendSV/HardFault handlers, interrupt control |
| `include/tinyrtos/kernel/scheduler.h` | **New** | Scheduler API |
| `src/kernel/scheduler.c` | **New** | Priority + round-robin selection |
| `include/tinyrtos/kernel/fault.h` | **New** | Fault handler API |
| `src/kernel/fault.c` | **New** | HardFault register capture + LED blink |
| `Makefile` | Modified | Added kernel sources, include path |
| `main.c` | Rewritten | RTOS demo with two LED blink tasks |

## Binary Size

```
   text     data      bss      dec
   3624        4     6320     9948   main.elf
```

- **Flash:** 3.6 KB (code + constants)
- **RAM:** 6.3 KB (5 TCBs × 1 KB stacks + static variables)
- **Remaining:** ~60 KB Flash, ~14 KB RAM available for application
