/**
 * @file    kernel.h
 * @brief   TinyRTOS kernel core API for STM32F103C8T6 (Cortex-M3).
 *
 * This is the public interface to the TinyRTOS kernel core.
 * It provides functions to:
 *   - Initialize the SysTick timer (1 kHz kernel tick)
 *   - Create tasks and start the kernel
 *   - Sleep (cooperative) or delay (busy-wait)
 *   - Yield the CPU to other tasks
 *   - Use nested critical sections (interrupt-safe)
 *   - Query tick count and CPU frequency
 *
 * The kernel core is the heart of the RTOS. It ties together:
 *   - SysTick timer (hardware) → periodic tick interrupt
 *   - Task management → TCB array, stack building
 *   - Scheduler → priority-based preemptive selection
 *   - PendSV handler → context switch (save/restore registers)
 *   - SVC handler → launch first task (switch from MSP to PSP)
 *
 * Boot sequence:
 *   1. reset_handler (core.S) → copies .data, zeros .bss, calls main()
 *   2. main() → clock_init(), create tasks, kernel_systick_init(), kernel_start()
 *   3. kernel_start() → SVC #0 → SVC_handler restores first task's context
 *   4. First task begins executing on its own stack (PSP)
 *   5. Every 1ms: SysTick_Handler → kernel_tick() → update_blocked() → PendSV
 *   6. PendSV_Handler: save R4-R11 of current task, scheduler picks next,
 *      restore R4-R11 of next task, hardware restores R0-R3/R12/LR/PC/xPSR
 *
 * Ported from Ian Wilkey's tinyrtos (Cortex-M7) to Cortex-M3 bare-metal.
 * Original: https://github.com/Ian-Wilkey/tinyrtos — MIT License
 */

#ifndef _RTOSK_KERNEL_H_
#define _RTOSK_KERNEL_H_

#include <stdint.h>

/**
 * TinyRTOS version string.
 */
#define RTOSK_KERNEL_VERSION "0.1.0"

/**
 * Function pointer type for a TinyRTOS task.
 *
 * Every task is a function with this signature:
 *   void my_task(void) {
 *       for (;;) {
 *           // ... do work ...
 *           rtosk_kernel_sleep_ms(100);  // yield CPU for 100ms
 *       }
 *   }
 *
 * Tasks must NEVER return. If a task returns, the LR register
 * in its initial stack frame points to rtosk_task_exit_trap(),
 * which is an infinite loop that catches this error safely.
 * Without the trap, returning from a task would cause the CPU
 * to pop garbage into PC and crash (likely HardFault).
 */
typedef void (*rtosk_task_func_t)(void);

/**
 * Forward declaration of the task info struct (defined in task.h).
 *
 * We forward-declare here so that kernel.h does not include task.h.
 * This keeps the include dependency one-directional:
 *   kernel.h  (standalone — declares task_func_t and forward-declares task_info)
 *   task.h    (includes kernel.h to reuse task_func_t)
 *
 * This prevents circular includes between kernel.h and task.h.
 */
typedef struct rtosk_task_info rtosk_task_info_t;

/* ================================================================== */
/*  Kernel lifecycle                                                    */
/* ================================================================== */

/**
 * Initialize the SysTick timer for a 1 ms kernel tick.
 *
 * This function configures the ARM Cortex-M3 SysTick timer, which is
 * a 24-bit down-counter built into every Cortex-M3 processor.
 *
 * What it does step by step:
 *   1. Calls rtosk_port_configure_exceptions() to set interrupt priorities:
 *      - PendSV = 0xFF (lowest) — context switches happen AFTER all ISRs
 *      - SysTick = 0xFE (one above PendSV) — tick runs BEFORE context switch
 *
 *   2. Sets SysTick->LOAD = (SystemCoreClock / 1000) - 1
 *      At 8 MHz HSI:  LOAD = 8000000/1000 - 1 = 7999 (counts 8000 cycles)
 *      At 72 MHz PLL: LOAD = 72000000/1000 - 1 = 71999 (counts 72000 cycles)
 *      Both give exactly 1 ms between tick interrupts.
 *
 *   3. Clears SysTick->VAL (resets the counter to LOAD value immediately)
 *
 *   4. Configures SysTick->CTRL:
 *      - CLKSOURCE = 1 (use processor clock, not external reference)
 *      - TICKINT = 1 (generate interrupt on underflow — this is what
 *        triggers SysTick_Handler every 1 ms)
 *      - ENABLE = 1 (start counting)
 *
 * How SysTick works:
 *   The counter decrements by 1 each clock cycle. When it reaches 0:
 *     - It reloads from LOAD on the next cycle
 *     - It sets the COUNTFLAG bit in CTRL
 *     - If TICKINT is set, it raises the SysTick exception (IRQ -1)
 *   SysTick_Handler (in port.c) then calls rtosk_kernel_tick().
 *
 * Must be called AFTER clock_init() (so SystemCoreClock is correct)
 * and BEFORE rtosk_kernel_start().
 *
 * Safe to call multiple times (idempotent — guards with a flag).
 */
void rtosk_kernel_systick_init(void);

/**
 * Start the TinyRTOS kernel. NEVER RETURNS.
 *
 * This is the point of no return — the CPU switches from bare-metal
 * mode to RTOS-managed multitasking.
 *
 * What it does step by step:
 *   1. Checks that at least one task has been created. If not, halts.
 *      (Starting the kernel with zero tasks would crash.)
 *
 *   2. Calls rtosk_port_configure_faults() to enable trap-on-divide-by-zero
 *      and trap-on-unaligned-access in SCB->CCR.
 *
 *   3. Creates the idle task — a special task at the LOWEST priority that
 *      runs an infinite WFI (Wait For Interrupt) loop. The idle task runs
 *      when no user task is READY. WFI puts the CPU into a low-power state
 *      until the next interrupt, saving energy.
 *
 *   4. Executes SVC #0 (Supervisor Call). This triggers the SVC_handler
 *      exception, which:
 *      a. Calls rtosk_task_get_stack_pointer() to get the first task's SP
 *      b. Restores R4-R11 from the task's initial stack frame
 *      c. Sets PSP to point past the restored R4-R11 (to the HW frame)
 *      d. Switches Thread mode to use PSP (sets CONTROL bit 2)
 *      e. Sets LR = 0xFFFFFFFD (EXC_RETURN: return to Thread mode using PSP)
 *      f. Returns via bx lr — the hardware pops R0-R3, R12, LR, PC, xPSR
 *         and the task starts executing!
 *
 * After this call:
 *   - MSP (Main Stack Pointer) is used by ISRs and exception handlers
 *   - PSP (Process Stack Pointer) points to the current task's stack
 *   - Each task has its own PSP value (switched by PendSV_Handler)
 *   - The main() function is abandoned — its stack frame is irrelevant
 *     since ISRs now use MSP (which still points to _estack)
 */
void rtosk_kernel_start(void);

/* ================================================================== */
/*  Task creation                                                       */
/* ================================================================== */

/**
 * Create a new task and add it to the kernel's task list.
 *
 * Tasks must be created BEFORE calling rtosk_kernel_start().
 * After the kernel starts, no more tasks can be added.
 *
 * What happens internally:
 *   1. A TCB (Task Control Block) is allocated from the static array.
 *      Up to RTOSK_MAX_TASKS (4) user tasks can be created.
 *   2. The task's stack (256 words = 1024 bytes) is filled with
 *      the watermark pattern 0xDEADBEEF for overflow detection.
 *   3. An initial exception stack frame is built at the TOP of the stack:
 *
 *      Stack layout (built top-down from stack[255]):
 *        xPSR   = 0x01000000 (Thumb bit — ARM requires this for Cortex-M)
 *        PC     = task_func   (where the task starts executing)
 *        LR     = rtosk_task_exit_trap (safety net if task returns)
 *        R12    = 0
 *        R3     = 0
 *        R2     = 0
 *        R1     = 0
 *        R0     = 0
 *        --- hardware-saved frame (8 words) ---
 *        R11    = 0
 *        R10    = 0
 *        R9     = 0
 *        R8     = 0
 *        R7     = 0
 *        R6     = 0
 *        R5    = 0
 *        R4     = 0
 *        --- software-saved frame (8 words) ---
 *        ← SP (task->sp) points here
 *
 *      When PendSV "restores" this context for the first time,
 *      it loads R4-R11 (all zeros, no harm), advances SP, and
 *      the hardware pops the exception frame, jumping to task_func.
 *
 *   4. The task state is set to READY and priority/name are stored.
 *
 * @param task_func  Function to run as the task (must not return).
 * @param priority   Higher value = higher priority (scheduler picks highest).
 * @param name       Human-readable name for debugging (stored by reference, not copied).
 */
void rtosk_kernel_create_task(rtosk_task_func_t task_func, uint32_t priority, const char * name);

/**
 * Get diagnostic information about a task (for debugging/monitoring).
 *
 * @param index  Task index (0..RTOSK_MAX_TASKS-1 for user tasks,
 *               RTOSK_IDLE_TASK_INDEX for the idle task).
 * @param info   Pointer to a rtosk_task_info_t struct to fill.
 * @return 1 on success, 0 on error (bad index or null pointer).
 */
uint32_t rtosk_kernel_get_task_info(uint32_t index, rtosk_task_info_t * info);

/* ================================================================== */
/*  Time and delay                                                      */
/* ================================================================== */

/**
 * Busy-wait delay for the specified number of milliseconds.
 *
 * This is a simple spin loop — it does NOT yield the CPU.
 * Other tasks cannot run during a delay_ms() call.
 *
 * Use case: short delays in initialization code (before the kernel starts)
 * or in ISRs where sleeping is not allowed.
 *
 * For a cooperative delay that lets other tasks run, use sleep_ms().
 *
 * @param ms  Number of milliseconds to wait.
 */
void rtosk_kernel_delay_ms(uint32_t ms);

/**
 * Cooperative sleep for the specified number of milliseconds.
 *
 * This is the primary timing mechanism for RTOS tasks.
 * Unlike delay_ms(), it YIELDS the CPU so other tasks can run.
 *
 * What happens step by step:
 *   1. Enters a critical section (disables interrupts).
 *      This prevents a SysTick interrupt from firing between
 *      reading the tick count and blocking the task.
 *
 *   2. Computes wake_tick = current_tick + ms.
 *      This is the future tick count at which the task should wake.
 *
 *   3. Blocks the current task:
 *      - Sets task state to RTOSK_TASK_BLOCKED
 *      - Stores wake_tick in the TCB
 *
 *   4. Exits the critical section (restores interrupt state).
 *
 *   5. Yields the CPU by triggering PendSV (rtosk_kernel_yield).
 *      The PendSV handler saves this task's context and switches
 *      to the next READY task.
 *
 *   6. When the system tick reaches wake_tick, the SysTick ISR
 *      calls rtosk_task_update_blocked(), which sets this task's
 *      state back to READY. The scheduler will then pick it up
 *      on the next context switch.
 *
 *   7. When this task resumes, execution continues right after
 *      the yield() call — as if sleep_ms() simply returned.
 *
 * @param ms  Number of milliseconds to sleep.
 */
void rtosk_kernel_sleep_ms(uint32_t ms);

/**
 * Get the current kernel tick count.
 *
 * Incremented by 1 every millisecond by the SysTick ISR.
 * Returns a 32-bit value that wraps around after ~49.7 days.
 *
 * @return  Number of SysTick interrupts since kernel start.
 */
uint32_t rtosk_kernel_get_ticks(void);

/**
 * Get the CPU clock frequency in Hz.
 *
 * Returns SystemCoreClock, which should be set by clock_init():
 *   - 8000000 (8 MHz) if running on HSI (default, no clock config)
 *   - 72000000 (72 MHz) if HSE + PLL is configured
 *
 * @return  System clock frequency in Hz.
 */
uint32_t rtosk_get_cpu_freq(void);

/* ================================================================== */
/*  Cooperative yield                                                   */
/* ================================================================== */

/**
 * Yield the CPU to another task of equal or higher priority.
 *
 * This sets the PENDSVSET bit in SCB->ICSR, which makes PendSV pending.
 * Since PendSV has the LOWEST interrupt priority (0xFF), the actual
 * context switch happens after all currently-running ISRs complete.
 *
 * If no other task is READY, the scheduler picks the same task
 * again (or the idle task), and execution resumes immediately.
 *
 * This function does NOT disable interrupts — the DSB + ISB
 * barriers ensure the PendSV takes effect, but ISRs can still
 * fire before PendSV actually runs.
 */
void rtosk_kernel_yield(void);

/* ================================================================== */
/*  Critical sections (interrupt-safe)                                  */
/* ================================================================== */

/**
 * Enter a critical section (disable interrupts).
 *
 * Critical sections protect shared data from being corrupted by
 * concurrent access from both task code and ISRs.
 *
 * Example of a race condition WITHOUT critical section:
 *   Task A reads RTOSK_SYSTICKS (e.g., 1000)
 *   ─── SysTick ISR fires, increments RTOSK_SYSTICKS to 1001 ───
 *   Task A computes wake_tick = 1000 + ms  ← used stale value!
 *
 * With critical section:
 *   enter_critical()  ← interrupts disabled
 *   Task A reads RTOSK_SYSTICKS (1000)
 *   (SysTick ISR cannot fire — interrupts disabled)
 *   Task A computes wake_tick = 1000 + ms
 *   exit_critical()   ← interrupts re-enabled
 *   (SysTick ISR fires now, but it's too late to cause a race)
 *
 * CRITICAL SECTIONS ARE NESTABLE. This is important because
 * sleep_ms() calls enter_critical() internally, and the caller
 * might already be in a critical section. The nesting counter
 * ensures interrupts are only re-enabled when the OUTERMOST
 * critical section exits.
 *
 * Example of nesting:
 *   enter_critical()     → nesting = 1, save PRIMASK, disable IRQ
 *     enter_critical()   → nesting = 2 (PRIMASK already saved)
 *       enter_critical() → nesting = 3
 *     exit_critical()    → nesting = 2 (not zero, keep IRQ disabled)
 *   exit_critical()      → nesting = 1 (not zero, keep IRQ disabled)
 *   exit_critical()      → nesting = 0 → restore original PRIMASK!
 *
 * IMPORTANT: Always pair enter and exit. Keep critical sections
 * as SHORT as possible — while in a critical section, the CPU
 * cannot respond to any interrupt (except NMI and HardFault).
 */
void rtosk_kernel_enter_critical(void);

/**
 * Exit a critical section (re-enable interrupts if outermost).
 *
 * Decrements the nesting counter. When it reaches 0 (outermost),
 * restores the PRIMASK value that was saved by the first
 * enter_critical() call. This correctly handles the case where
 * interrupts were already disabled before entering (e.g., from
 * an ISR that calls a kernel function).
 */
void rtosk_kernel_exit_critical(void);

#endif /* _RTOSK_KERNEL_H_ */
