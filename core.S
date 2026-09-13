/*
 * Startup code for STM32F103C8T6 (Blue Pill).
 *
 * Based on Vivonomicon's "Bare Metal STM32 Programming Part 2"
 * (https://vivonomicon.com/2018/04/20/bare-metal-stm32-programming-part-2-making-it-to-main/)
 * adapted for the STM32F1 Cortex-M3.
 *
 * This is the reset handler — the first code that runs after
 * reset. It performs three tasks before jumping to C code:
 *   1. Set the stack pointer
 *   2. Copy .data initializers from Flash to RAM
 *   3. Zero out the .bss section
 *   4. Branch to main()
 *
 * Open source under the MIT License
 */

.syntax unified
.cpu cortex-m3
.fpu softvfp
.thumb

// Global values.
.global reset_handler

/*
 * The Reset handler. Called on reset (power-on, NRST pin, etc.).
 */
.type reset_handler, %function
reset_handler:
  // Set the stack pointer to the end of the stack.
  // The '_estack' value is defined in our linker script.
  // (On Cortex-M this is also done automatically from the vector
  //  table, but we set it explicitly for clarity.)
  LDR  r0, =_estack
  MOV  sp, r0

  // Copy data from flash to RAM data init section.
  // R2 will store our progress along the sidata section.
  MOVS r0, #0
  // Load the start/end addresses of the data section,
  // and the start of the data init section.
  LDR  r1, =_sdata
  LDR  r2, =_edata
  LDR  r3, =_sidata
  B    copy_sidata_loop

  copy_sidata:
    // Offset the data init section by our copy progress.
    LDR  r4, [r3, r0]
    // Copy the current word into data, and increment.
    STR  r4, [r1, r0]
    ADDS r0, r0, #4

  copy_sidata_loop:
    // Unless we've copied the whole data section, copy the
    // next word from sidata->data.
    ADDS r4, r0, r1
    CMP  r4, r2
    BCC  copy_sidata

  // Once we are done copying the data section into RAM,
  // move on to filling the BSS section with 0s.
  MOVS r0, #0
  LDR  r1, =_sbss
  LDR  r2, =_ebss
  B    reset_bss_loop

  // Zero out the BSS segment.
  reset_bss:
    // Store a 0 and increment by a word.
    STR  r0, [r1]
    ADDS r1, r1, #4

  reset_bss_loop:
    // We'll use R1 to count progress here; if we aren't
    // done, reset the next word and increment.
    CMP  r1, r2
    BCC  reset_bss

  // Branch to the 'main' method in C.
  // Using B (not BL) because main should never return.
  B    main
.size reset_handler, .-reset_handler
