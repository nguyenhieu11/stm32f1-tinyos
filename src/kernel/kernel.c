/**
 * @file    kernel.c
 * @brief   TinyRTOS kernel core — SysTick init, tick handler, sleep, critical sections.
 *
 * This is the central nervous system of the RTOS. It manages:
 *
 *   1. SysTick timer initialization (1 kHz tick from hardware timer)
 *   2. The global tick counter (RTOSK_SYSTICKS, incremented every 1 ms)
 *   3. Task creation (delegates to the task module)
 *   4. Kernel startup (launches first task via SVC)
 *   5. Cooperative sleep (sleep_ms — blocks task, yields CPU)
 *   6. Busy-wait delay (delay_ms — spins, does not yield)
 *   7. Nested critical sections (PRIMASK-based interrupt disable/enable)
 *   8. The tick ISR callback (called from SysTick_Handler every 1 ms)
 *
 * Data flow of a typical tick:
 *
 *   Hardware: SysTick counter reaches 0
 *       ↓
 *   CPU: Enters SysTick_Handler (exception, uses MSP)
 *       ↓
 *   port.c: SysTick_handler() calls rtosk_kernel_tick()
 *       ↓
 *   kernel.c: rtosk_kernel_tick()
 *       ├── Increments RTOSK_SYSTICKS
 *       ├── Calls rtosk_task_update_blocked(RTOSK_SYSTICKS)
 *       │   └── Scans BLOCKED tasks, sets READY if wake_tick reached
 *       └── Calls rtosk_port_yield()
 *           └── Sets PendSV pending (SCB->ICSR = PENDSVSET)
 *               ↓
 *   CPU: (after SysTick returns) Enters PendSV_Handler
 *       ↓
 *   port.c: pend_SV_handler()
 *       ├── Saves R4-R11 of current task
 *       ├── Calls rtosk_scheduler_select_next()
 *       │   └── Picks highest-priority READY task
 *       └── Restores R4-R11 of next task
 *       ↓
 *   CPU: Hardware pops exception frame → next task resumes
 *
 * Reference: https://github.com/Ian-Wilkey/tinyrtos — MIT License
 */

#include <tinyrtos/kernel/kernel.h>
#include <tinyrtos/kernel/task.h>
#include <tinyrtos/kernel/port.h>

#include "stm32f103x8.h"

/* ================================================================== */
/*  Static (private) kernel state                                       */
/* ================================================================== */

/**
 * Global tick counter — incremented every 1 ms by rtosk_kernel_tick().
 *
 * Declared volatile because it is modified in an ISR (SysTick_Handler)
 * and read from task code. Without volatile, the compiler might cache
 * the value in a register and never re-read it from memory.
 *
 * A uint32_t at 1 kHz wraps after 2^32 / 1000 / 3600 / 24 ≈ 49.7 days.
 * The sleep_ms function uses signed comparison to handle wraparound.
 */
static volatile uint32_t RTOSK_SYSTICKS = 0UL;

/**
 * Guard flag: prevents rtosk_kernel_systick_init() from running twice.
 * Calling SysTick init twice would restart the timer and cause a
 * timing glitch (the counter resets to LOAD, potentially skipping
 * a tick or double-counting).
 */
static uint8_t RTOSK_KERNEL_SYSTICK_INIT = 0U;

/**
 * Guard flag: prevents rtosk_kernel_start() from running twice.
 * Starting the kernel twice is undefined — the SVC handler would
 * try to launch a task while tasks are already running.
 */
static uint8_t RTOSK_KERNEL_INIT = 0U;

/**
 * Critical section nesting counter.
 *
 * Tracks how many nested critical sections are active.
 * Interrupts are disabled when this is > 0, re-enabled only
 * when it returns to 0 (the outermost exit_critical).
 *
 * Example:
 *   enter() → nesting = 1 → save PRIMASK, disable IRQ
 *   enter() → nesting = 2 → (already disabled, no-op on PRIMASK)
 *   exit()  → nesting = 1 → (still > 0, don't re-enable)
 *   exit()  → nesting = 0 → restore saved PRIMASK → IRQ re-enabled
 */
static uint32_t RTOSK_CRITICAL_NESTING = 0UL;

/**
 * Saved PRIMASK value from the outermost critical section entry.
 *
 * PRIMASK is a 1-bit ARM register:
 *   0 = interrupts enabled (normal operation)
 *   1 = interrupts disabled (only NMI/HardFault can preempt)
 *
 * We save the ORIGINAL value on the first enter_critical() and
 * restore it on the last exit_critical(). This correctly handles
 * the case where interrupts were already disabled before entering
 * (e.g., code called from within an ISR).
 */
static uint32_t RTOSK_CRITICAL_PRIMASK = 0UL;

/* ================================================================== */
/*  SysTick initialization                                              */
/* ================================================================== */

/**
 * Initialize the SysTick timer for a 1 ms kernel tick.
 *
 * The SysTick timer is a 24-bit down-counter built into every
 * Cortex-M3 processor (ARMv7-M architecture requirement). It provides
 * a standardized, portable way to generate periodic interrupts.
 *
 * Register overview (SysTick_Type at 0xE000E010):
 *
 *   CTRL (0x00): Control and status
 *     bit 0 (ENABLE):   Counter active when 1
 *     bit 1 (TICKINT):  Generate interrupt on underflow when 1
 *     bit 2 (CLKSOURCE): 1 = processor clock, 0 = external reference
 *     bit 16 (COUNTFLAG): Set to 1 when counter reaches 0 (sticky)
 *
 *   LOAD (0x04): Reload value (24-bit)
 *     When the counter reaches 0, it reloads this value on the next cycle.
 *     For a 1ms tick: LOAD = (SystemCoreClock / 1000) - 1
 *     At 8 MHz:  LOAD = 7999   → counts 8000 cycles → 1.000 ms
 *     At 72 MHz: LOAD = 71999  → counts 72000 cycles → 1.000 ms
 *
 *   VAL (0x08): Current counter value
 *     Writing any value resets the counter to LOAD on the next cycle.
 *     This ensures a clean start (don't count down from a stale value).
 *
 *   CALIB (0x0C): Calibration value (read-only)
 *     Some chips store a 10ms reference value here, but it's often
 *     unreliable. We ignore it and compute LOAD from SystemCoreClock.
 *
 * Timer operation:
 *   1. Counter starts at LOAD and decrements by 1 each clock cycle
 *   2. When counter reaches 0:
 *      a. COUNTFLAG bit is set in CTRL (readable, sticky)
 *      b. Counter reloads from LOAD on the next cycle
 *      c. If TICKINT=1, SysTick exception is raised (IRQ -1)
 *   3. The cycle repeats indefinitely
 *
 * Interrupt priority:
 *   SysTick is set to priority 0xFE (one above PendSV at 0xFF).
 *   This means: SysTick runs before any PendSV, ensuring the tick
 *   counter is updated and blocked tasks are woken BEFORE the
 *   scheduler selects the next task to run.
 *
 *   Priority order (lower number = higher priority):
 *     0x00 ─── Highest priority (most ISRs)
 *      ...
 *     0x03 ─── EXTI, UART, etc.
 *      ...
 *     0xFE ─── SysTick (kernel tick)
 *     0xFF ─── PendSV (context switch — lowest priority)
 */
void rtosk_kernel_systick_init(void) {
    /* Idempotent guard: do nothing if already initialized */
    if (RTOSK_KERNEL_SYSTICK_INIT) return;

    /*
     * Step 1: Configure exception priorities.
     *
     * Sets PendSV to 0xFF (lowest) and SysTick to 0xFE.
     * This ensures the kernel tick runs before context switches.
     *
     * NOTE: On Cortex-M3, the priority register for system exceptions
     * (PendSV, SysTick, etc.) is in SCB->SHP[], not NVIC->IP[].
     * NVIC_SetPriority() handles this automatically for negative IRQns.
     */
    rtosk_port_configure_exceptions();

    /*
     * Step 2: Set the reload value for a 1 ms tick.
     *
     * SystemCoreClock is the CPU frequency in Hz (set by clock_init).
     * Dividing by 1000 gives cycles per millisecond.
     * Subtracting 1 because the counter counts from LOAD DOWN TO 0
     * (inclusive), so it counts LOAD+1 cycles total.
     *
     * Example at 8 MHz HSI (no PLL):
     *   LOAD = 8000000 / 1000 - 1 = 7999
     *   Counter: 7999 → 7998 → ... → 1 → 0 → (interrupt, reload to 7999)
     *   That's 8000 cycles = 1.000 ms at 8 MHz.
     */
    SysTick->LOAD = (SystemCoreClock / 1000UL) - 1UL;

    /*
     * Step 3: Clear the current counter value.
     *
     * Writing any value to VAL resets it to LOAD immediately.
     * This ensures we start from a known state (not a stale
     * value left from a previous configuration).
     */
    SysTick->VAL = 0UL;

    /*
     * Step 4: Configure and start the SysTick timer.
     *
     * CTRL register bits:
     *   CLKSOURCE (bit 2) = 1: Use processor clock (not external).
     *     On STM32F103, the processor clock = SYSCLK (HSE or HSI).
     *     External reference (bit 2 = 0) would use STCLK, which is
     *     not connected on most STM32 boards.
     *
     *   TICKINT (bit 1) = 1: Generate interrupt on underflow.
     *     Without this, the timer just counts silently and we'd
     *     have to poll COUNTFLAG — wasteful and imprecise.
     *
     *   ENABLE (bit 0) = 1: Start the counter.
     *     The counter begins decrementing from LOAD immediately.
     */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |  /* Processor clock */
                    SysTick_CTRL_TICKINT_Msk   |  /* Interrupt on underflow */
                    SysTick_CTRL_ENABLE_Msk;       /* Start counting */

    /* Mark as initialized (prevent double-init) */
    RTOSK_KERNEL_SYSTICK_INIT = 1U;
}

/* ================================================================== */
/*  Kernel start                                                        */
/* ================================================================== */

/**
 * Start the TinyRTOS kernel. NEVER RETURNS.
 *
 * This function performs the final setup and launches the first task
 * by triggering a Supervisor Call (SVC) exception.
 *
 * The SVC mechanism is used because:
 *   - We need to switch from MSP to PSP (changing the stack pointer
 *     requires privileged access, which we have in Handler mode).
 *   - We need to "return" to the first task as if it was interrupted
 *     (exception return mechanism via bx lr with EXC_RETURN value).
 *   - SVC provides a clean, well-defined way to do this.
 *
 * After this call, the CPU is in Thread mode using PSP.
 * MSP is used only by ISR/exception handlers.
 * main() is abandoned — its stack frame is irrelevant.
 */
void rtosk_kernel_start(void) {
    /* Idempotent guard */
    if (RTOSK_KERNEL_INIT) return;

    /*
     * Sanity check: at least one task must exist.
     *
     * If we started the kernel with zero tasks:
     *   1. rtosk_task_init_idle() would create the idle task
     *   2. SVC handler would try to restore the idle task's context
     *   3. The idle task would run... but there's nothing useful
     *      happening. This is actually OK, but it's likely a bug
     *      in the user's code (forgot to call create_task).
     *   So we halt with an explicit error rather than silently idle.
     */
    if (rtosk_task_get_count() == 0UL) {
        /* No tasks created — halt with a visible error */
        for (;;) {}
    }

    /*
     * Enable fault traps.
     *
     * DIV_0_TRP:  Causes UsageFault on integer divide by zero.
     *             Without this, ARM silently returns 0 from div-by-zero.
     *
     * UNALIGN_TRP: Causes UsageFault on unaligned word/halfword access.
     *              Cortex-M3 doesn't support unaligned word access —
     *              without the trap, the access produces wrong results
     *              silently (or faults in some cases).
     */
    rtosk_port_configure_faults();

    /*
     * Create the idle task.
     *
     * The idle task runs when no user task is READY. It executes
     * an infinite WFI (Wait For Interrupt) loop, which puts the
     * CPU into a low-power sleep state until the next interrupt.
     *
     * The idle task is always at priority 0 (lowest) and is placed
     * at RTOSK_IDLE_TASK_INDEX (slot 4) in the TCB array.
     */
    rtosk_task_init_idle();

    /* Mark kernel as initialized */
    RTOSK_KERNEL_INIT = 1U;

    /*
     * Launch the first task via SVC #0.
     *
     * This triggers the SVC_handler exception, which:
     *   1. Gets the first task's stack pointer
     *   2. Restores R4-R11 from the initial stack frame
     *   3. Sets PSP to the task's stack
     *   4. Switches to PSP for Thread mode
     *   5. Returns via bx lr → hardware pops R0-R3, R12, LR, PC, xPSR
     *
     * After this instruction, we never return to this function.
     * The CPU is now executing the first task on its own PSP stack.
     *
     * NOTE: This is called via __asm volatile, not as a C function
     * call, because:
     *   - SVC is a special instruction that causes an exception
     *   - We need it to happen exactly here, not potentially
     *     relocated by the compiler
     *   - The "volatile" prevents the compiler from optimizing it away
     */
    rtosk_port_start_first_task();
}

/* ================================================================== */
/*  Task creation (thin wrappers)                                       */
/* ================================================================== */

/**
 * Create a new task — delegates to the task module.
 *
 * This is a thin wrapper that provides a clean kernel-level API.
 * The actual work (TCB allocation, stack building, watermark) is
 * done by rtosk_task_create() in task.c.
 *
 * The wrapper pattern is intentional: it keeps the public API
 * in the kernel namespace (rtosk_kernel_*) while the internal
 * implementation details are in the task namespace (rtosk_task_*).
 */
void rtosk_kernel_create_task(rtosk_task_func_t task_func, uint32_t priority, const char * name) {
    rtosk_task_create(task_func, priority, name);
}

/**
 * Get task diagnostic information — delegates to the task module.
 */
uint32_t rtosk_kernel_get_task_info(uint32_t index, rtosk_task_info_t * info) {
    return rtosk_task_get_info(index, info);
}

/* ================================================================== */
/*  Time and delay                                                      */
/* ================================================================== */

/**
 * Busy-wait delay for the specified milliseconds.
 *
 * This is a simple spin loop that polls the global tick counter.
 * It does NOT yield the CPU — no other task can run during delay_ms().
 *
 * The subtraction (RTOSK_SYSTICKS - start) handles uint32_t wraparound
 * correctly due to unsigned arithmetic. For example, if start = 0xFFFFFFF0
 * and RTOSK_SYSTICKS = 0x10, then:
 *   0x10 - 0xFFFFFFF0 = 0x20 (unsigned) = 32 ticks — correct!
 *
 * Use cases:
 *   - Short delays during initialization (before kernel starts)
 *   - Delays in ISR context (where sleeping is forbidden)
 *   - Situations where you explicitly don't want context switches
 *
 * For everything else, prefer rtosk_kernel_sleep_ms().
 */
void rtosk_kernel_delay_ms(uint32_t ms) {
    uint32_t start = RTOSK_SYSTICKS;
    while ((RTOSK_SYSTICKS - start) < ms)
        ;  /* spin */
}

/**
 * Cooperative sleep for the specified milliseconds.
 *
 * This is the primary timing mechanism for RTOS tasks. Unlike delay_ms(),
 * it YIELDS the CPU so other tasks can run during the wait.
 *
 * The sequence of events for sleep_ms(100):
 *   1. Enter critical section (disable IRQ — prevents tick race)
 *   2. Read current tick (e.g., 500)
 *   3. Compute wake_tick = 500 + 100 = 600
 *   4. Block current task with wake_tick = 600
 *   5. Exit critical section (restore IRQ)
 *   6. Yield (trigger PendSV → context switch to another task)
 *   ... time passes, SysTick increments the counter ...
 *   7. At tick 600, SysTick ISR calls update_blocked() → task becomes READY
 *   8. Scheduler picks this task on the next PendSV
 *   9. Task resumes here — sleep_ms() returns
 *
 * Why enter_critical:
 *   Without the critical section, a SysTick interrupt could fire
 *   between reading RTOSK_SYSTICKS and blocking the task. In that
 *   case, the wake_tick would be based on a stale tick value, and
 *   the task might sleep 1ms longer than intended. The critical
 *   section prevents this race condition.
 *
 * Note: sleep_ms(0) is valid — it blocks the task and yields,
 * giving other equal-priority tasks a chance to run.
 */
void rtosk_kernel_sleep_ms(uint32_t ms) {
    rtosk_kernel_enter_critical();
    uint32_t wake_tick = RTOSK_SYSTICKS + ms;
    rtosk_task_block_current_until(wake_tick);
    rtosk_kernel_exit_critical();
    rtosk_kernel_yield();
}

/**
 * Get the current kernel tick count (milliseconds since kernel start).
 */
uint32_t rtosk_kernel_get_ticks(void) {
    return RTOSK_SYSTICKS;
}

/**
 * Get the CPU clock frequency in Hz.
 */
uint32_t rtosk_get_cpu_freq(void) {
    return SystemCoreClock;
}

/* ================================================================== */
/*  Cooperative yield                                                   */
/* ================================================================== */

/**
 * Yield the CPU — trigger PendSV for a context switch.
 *
 * Delegates to rtosk_port_yield() which:
 *   1. Sets PENDSVSET bit in SCB->ICSR (makes PendSV pending)
 *   2. Executes DSB (data sync barrier — ensure the write completes)
 *   3. Executes ISB (instruction sync barrier — flush pipeline)
 *
 * The actual context switch doesn't happen immediately — PendSV
 * has the LOWEST priority (0xFF), so it waits until all other ISRs
 * complete. This is the correct behavior: we don't want a context
 * switch to interrupt an ISR.
 */
void rtosk_kernel_yield(void) {
    rtosk_port_yield();
}

/* ================================================================== */
/*  Critical sections (PRIMASK-based nesting)                           */
/* ================================================================== */

/**
 * Enter a critical section — disable interrupts with nesting support.
 *
 * How PRIMASK-based critical sections work:
 *
 * PRIMASK is a special ARM register (part of the Cortex-M3 program model):
 *   Bit 0 = 0: All interrupts enabled (normal)
 *   Bit 0 = 1: All interrupts disabled (except NMI and HardFault)
 *
 * The flow:
 *   1. rtosk_port_irq_save():
 *      a. Reads current PRIMASK into 'primask'
 *      b. Sets PRIMASK = 1 (disable IRQ) via "cpsid i" instruction
 *      c. Executes DSB + ISB (ensures the disable takes effect NOW)
 *      d. Returns the OLD primask value (0 if IRQ was enabled, 1 if disabled)
 *
 *   2. If this is the FIRST (outermost) entry (nesting == 0):
 *      Save the old PRIMASK value — we'll need it when we exit.
 *      This correctly handles the case where interrupts were ALREADY
 *      disabled before entering (e.g., called from an ISR).
 *
 *   3. Increment nesting counter.
 *
 * Why nesting is needed:
 *   sleep_ms() enters a critical section internally. If the caller
 *   is already in a critical section, the exit in sleep_ms() must
 *   NOT re-enable interrupts — only the outermost exit should do that.
 */
void rtosk_kernel_enter_critical(void) {
    uint32_t primask = rtosk_port_irq_save();

    if (RTOSK_CRITICAL_NESTING == 0UL) {
        /* First entry: save the original interrupt state */
        RTOSK_CRITICAL_PRIMASK = primask;
    }
    RTOSK_CRITICAL_NESTING++;
}

/**
 * Exit a critical section — re-enable interrupts when outermost.
 *
 * Decrements the nesting counter. When it reaches 0 (the matching
 * outermost enter_critical), restores the saved PRIMASK.
 *
 * Safety check: if nesting is already 0, do nothing (extra exit
 * is a no-op rather than undefined behavior).
 */
void rtosk_kernel_exit_critical(void) {
    if (RTOSK_CRITICAL_NESTING == 0UL) return;  /* underflow guard */

    RTOSK_CRITICAL_NESTING--;

    if (RTOSK_CRITICAL_NESTING == 0UL) {
        /* Outermost exit: restore original interrupt state */
        rtosk_port_irq_restore(RTOSK_CRITICAL_PRIMASK);
    }
}

/* ================================================================== */
/*  Kernel tick handler (called from SysTick_Handler every 1 ms)        */
/* ================================================================== */

/**
 * Process one kernel tick — the heartbeat of the RTOS.
 *
 * Called by SysTick_Handler (in port.c) every 1 ms.
 * This function does two things:
 *
 *   1. Increments the global tick counter (RTOSK_SYSTICKS).
 *      This is the time base for all sleep/wake operations.
 *
 *   2. Wakes any BLOCKED tasks whose wake_tick has arrived.
 *      rtosk_task_update_blocked() scans all user tasks:
 *        - If task state == BLOCKED and (current_tick - wake_tick) >= 0:
 *          → Set state to READY
 *      The signed comparison handles uint32_t wraparound correctly.
 *
 *   3. Triggers a context switch (PendSV) so the scheduler can
 *      pick the highest-priority READY task.
 *
 * NOTE: This function runs in ISR context (SysTick exception).
 * It should be fast. The actual context switch (PendSV) happens
 * after SysTick returns, because PendSV has lower priority.
 *
 * The guard (RTOSK_KERNEL_INIT check) prevents calling the task
 * module before the kernel is fully started. Before kernel_start(),
 * only the tick counter is incremented.
 */
void rtosk_kernel_tick(void) {
    RTOSK_SYSTICKS++;

    /* Before kernel_start(), only count ticks — no task management */
    if (RTOSK_KERNEL_INIT == 0U) return;

    /* Wake any BLOCKED tasks whose sleep has expired */
    rtosk_task_update_blocked(RTOSK_SYSTICKS);

    /* Trigger a context switch so the scheduler can run */
    rtosk_port_yield();
}
