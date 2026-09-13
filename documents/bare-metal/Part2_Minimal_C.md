# Part 2: Minimal C — Making it to `main()`

## Overview

In [Part 1](Part1_Minimal.md), we had the absolute minimum: a 2-word vector table and a reset handler in pure assembly that loaded `0xDEADBEEF` into a register and looped. There was no C code, no `.data`/`.bss` initialization, and no linker `SECTIONS` block.

In this part, we make the jump to C. The reset handler now does three critical jobs before branching to a C `main()` function:

1. **Set the stack pointer**
2. **Copy `.data` initializers from Flash to RAM**
3. **Zero out the `.bss` section**

We also add a full vector table for the STM32F103C8T6 and test where different variable types end up in memory.

**Reference**: [Vivonomicon Part 2 — Making it to Main](https://vivonomicon.com/2018/04/20/bare-metal-stm32-programming-part-2-making-it-to-main/)

---

## What Changed from Part 1

| File | Part 1 | Part 2 |
|---|---|---|
| `core.S` | 2-word vector table + simple loop | Reset handler only (`.data`/`.bss` init → `main`) |
| `vector_table.S` | *(did not exist)* | Full 59-entry vector table for STM32F103C8T6 |
| `main.c` | *(did not exist)* | C `main()` with variable type tests |
| `STM32F103C8T6.ld` | `MEMORY` block only (no `SECTIONS`) | Full `SECTIONS` block mapping all sections |
| `Makefile` | Assembly only | Assembly + C compilation |

---

## Step 1: The Vector Table (`vector_table.S`)

The vector table is the first thing the processor reads after reset. It must be placed at `0x08000000` (start of Flash).

### Cortex-M3 Vector Table Layout

The STM32F103C8T6 (medium-density) has **59 vector table entries**:

| Position | Address | Content |
|---|---|---|
| 0 | `0x08000000` | Initial Stack Pointer (`_estack = 0x20005000`) |
| 1 | `0x08000004` | Reset Handler (`reset_handler`) |
| 2 | `0x08000008` | NMI Handler |
| 3 | `0x0800000C` | HardFault Handler |
| 4 | `0x08000010` | MemManage Handler |
| 5 | `0x08000014` | BusFault Handler |
| 6 | `0x08000018` | UsageFault Handler |
| 7-10 | `0x0800001C-0x08000028` | Reserved |
| 11 | `0x0800002C` | SVCall Handler |
| 12 | `0x08000030` | Debug Monitor |
| 13 | `0x08000034` | Reserved |
| 14 | `0x08000038` | PendSV Handler |
| 15 | `0x0800003C` | SysTick Handler |
| 16-58 | `0x08000040-0x080000EC` | STM32F103 peripheral IRQs (43 entries) |

### Weak Aliases

All interrupt handlers are declared as **weak aliases** to `default_interrupt_handler` (an infinite loop):

```asm
.weak NMI_handler
.thumb_set NMI_handler,default_interrupt_handler
```

This means if you define a function called `NMI_handler` in your C code, it will override the default. The linker resolves the strong symbol (your C function) over the weak one (the infinite loop).

### What the Processor Does on Reset

The Cortex-M3 hardware automatically (ARM DDI 0337E, Section 3.2.1):

1. Reads word at `0x08000000` → loads into SP (stack pointer = `0x20005000`)
2. Reads word at `0x08000004` → loads into PC (program counter = `reset_handler`)
3. Begins execution at `reset_handler`

---

## Step 2: The Reset Handler (`core.S`)

The reset handler is the first code that runs. It performs three tasks before jumping to C:

### Task 1: Set the Stack Pointer

```asm
LDR  r0, =_estack
MOV  sp, r0
```

This is technically redundant (the hardware already loaded SP from the vector table), but it's done explicitly for clarity and safety.

### Task 2: Copy `.data` from Flash to RAM

```asm
MOVS r0, #0          // r0 = offset counter
LDR  r1, =_sdata     // r1 = start of .data in RAM
LDR  r2, =_edata     // r2 = end of .data in RAM
LDR  r3, =_sidata    // r3 = start of init values in Flash
B    copy_sidata_loop

copy_sidata:
  LDR  r4, [r3, r0]  // Load word from Flash
  STR  r4, [r1, r0]  // Store word to RAM
  ADDS r0, r0, #4    // Next word

copy_sidata_loop:
  ADDS r4, r0, r1    // r4 = current RAM address
  CMP  r4, r2        // Have we reached the end?
  BCC  copy_sidata   // If not, copy next word
```

**Why?** Initialized variables (like `int x = 42;`) have their initial values stored in Flash. At runtime, they need to live in RAM so they can be modified. The reset handler copies them word-by-word from Flash (`_sidata`) to RAM (`_sdata` → `_edata`).

### Task 3: Zero `.bss`

```asm
MOVS r0, #0          // r0 = 0 (the value to store)
LDR  r1, =_sbss      // r1 = start of .bss in RAM
LDR  r2, =_ebss      // r2 = end of .bss in RAM
B    reset_bss_loop

reset_bss:
  STR  r0, [r1]      // Store 0
  ADDS r1, r1, #4    // Next word

reset_bss_loop:
  CMP  r1, r2        // Have we reached the end?
  BCC  reset_bss     // If not, zero next word
```

**Why?** The C standard guarantees that uninitialized global/static variables are zero at program start. Rather than storing zeros in Flash (wasting space), the linker puts them in `.bss` and the reset handler zeros them.

### Task 4: Branch to `main`

```asm
B    main
```

Uses `B` (branch) instead of `BL` (branch-with-link) because `main` should never return. If it does, execution falls off into undefined behavior.

---

## Step 3: The Linker Script (`STM32F103C8T6.ld`)

The linker script now has a full `SECTIONS` block that tells the linker where to put everything.

### Memory Map

```
MEMORY
{
    FLASH ( rx )  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM ( rxw )   : ORIGIN = 0x20000000, LENGTH = 20K
}
```

### Section Placement

```
Flash (0x08000000):
  ┌─────────────────────┐
  │  .vector_table      │  ← Processor reads SP and reset handler from here
  ├─────────────────────┤
  │  .text              │  ← Program code (reset_handler, main, etc.)
  ├─────────────────────┤
  │  .rodata            │  ← Read-only data (const variables, strings)
  ├─────────────────────┤
  │  _sidata            │  ← Initial values for .data (copied to RAM at startup)
  └─────────────────────┘

RAM (0x20000000):
  ┌─────────────────────┐
  │  .data              │  ← Initialized variables (copied from Flash)
  ├─────────────────────┤
  │  .bss               │  ← Zero-initialized variables
  ├─────────────────────┤
  │  .dynamic_allocations│  ← Reserved for heap/stack (1KB guard)
  ├─────────────────────┤
  │                     │
  │  Stack (grows down) │
  │         ↓           │
  └─────────────────────┘  ← _estack = 0x20005000
```

### Key Linker Script Concepts

**`_sidata = .;`** — This symbol records the current address (end of Flash content) as where `.data` init values will be stored. The `AT(_sidata)` directive tells the linker that the **load address** (in Flash) differs from the **virtual address** (in RAM).

**`KEEP (*(.vector_table))`** — The `KEEP` directive prevents the linker from discarding the vector table section, even though no code explicitly references it. Without `KEEP`, the linker's garbage collection would remove it.

**`_Min_Leftover_RAM = 0x400`** — Reserves 1KB for stack/heap. If the linker runs out of RAM, it will generate an error at link time rather than producing mysterious runtime failures.

---

## Step 4: The Makefile

The Makefile now supports C compilation:

```makefile
# C compilation directives
CFLAGS += -mcpu=$(MCU_SPEC)    # Target CPU
CFLAGS += -mthumb              # Thumb instruction set
CFLAGS += -Wall                # All warnings
CFLAGS += -g                   # Debug symbols
CFLAGS += --specs=nosys.specs  # Ignore semihosting

# Source files
AS_SRC  = ./core.S
AS_SRC += ./vector_table.S
C_SRC   = ./main.c

# Pattern rule for C files
%.o: %.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@
```

Key flags:
- **`--specs=nosys.specs`**: Tells the compiler to ignore semihosting dependencies (we don't have an OS)
- **`-nostdlib`**: Don't link the standard library (not needed for bare metal)
- **`-lgcc`**: Link only the GCC runtime support library (needed for soft-float, etc.)

---

## Step 5: Variable Type Tests (`main.c`)

We added various variable types to demonstrate where each one ends up in memory.

### Variable Types and Their Placement

| Variable | Type | Section | Location | Why |
|---|---|---|---|---|
| `global_init = 42` | Global initialized | `.data` | RAM | Non-zero init value, copied from Flash |
| `global_init_neg = -1` | Global initialized | `.data` | RAM | Non-zero init value |
| `global_char = 'A'` | Global initialized | `.data` | RAM | Non-zero init value |
| `global_uninit` | Global uninitialized | `.bss` | RAM | Defaults to zero |
| `global_uninit_array[4]` | Global uninitialized | `.bss` | RAM | Defaults to zero |
| `global_zero = 0` | Global zero-init | `.bss` | RAM | Explicit zero = same as uninitialized |
| `global_zero_array[3]` | Global zero-init | `.bss` | RAM | Explicit zero |
| `global_const = 100` | Global const | `.rodata` | Flash | Read-only, never changes |
| `global_const_str[]` | Global const | `.rodata` | Flash | Read-only string |
| `static_init = 99` | Static initialized | `.data` | RAM | Same as global, file-scoped |
| `static_init_arr[2]` | Static initialized | `.data` | RAM | Same as global |
| `static_uninit` | Static uninitialized | `.bss` | RAM | Same as global |
| `static_zero = 0` | Static zero-init | `.bss` | RAM | Same as global |
| `local_var` | Local | Stack | RAM | Allocated on function entry |
| `local_uninit` | Local | Stack | RAM | Allocated on function entry |
| `local_char` | Local | Stack | RAM | Allocated on function entry |
| `local_const` | Local const | Stack | RAM | May be optimized to immediate |
| `local_static_uninit` | Static local | `.bss` | RAM | Static storage duration |
| `local_static_zero` | Static local | `.bss` | RAM | Static storage duration |
| `local_static_init = 77` | Static local | `.data` | RAM | Static storage, non-zero init |

### Key Insight: `static` Inside a Function

A `static` variable inside a function has **static storage duration** — it lives in `.bss` or `.data`, not on the stack. It persists across function calls, just like a global variable, but its name is only visible inside the function.

---

## Build and Verification

### Build Output

```
$ make clean && make
   text    data     bss     dec     hex filename
    384      28    1088    1500     5dc main.elf
```

- **text** (384 bytes): Code + read-only data in Flash
- **data** (28 bytes): Initialized variables (init values stored in Flash, copied to RAM)
- **bss** (1088 bytes): Zero-initialized variables in RAM (includes 1024-byte `.dynamic_allocations` guard)

### Symbol Table Verification

```
$ arm-none-eabi-nm -n main.elf
```

#### Flash Region (`0x08000000`)

```
08000000 R vtable              ← Vector table at start of Flash
080000ec T reset_handler       ← Reset handler code
08000134 T main                ← C main() function
0800016c T default_interrupt_handler  ← Default IRQ handler (infinite loop)
08000170 R global_const        ← const int = 100 (in .rodata)
08000174 R global_const_str    ← const char[] = "Hello STM32" (in .rodata)
08000180 R _sidata             ← Start of .data init values in Flash
```

#### RAM Region — `.data` Section (`0x20000000`)

```
20000000 D _sdata              ← Start of .data section
20000000 D global_init         ← int = 42
20000004 D global_init_neg     ← int = -1
20000008 D global_char         ← char = 'A' (0x41)
2000000c d static_init         ← static int = 99
20000010 d static_init_arr     ← static int[2] = {10, 20}
20000018 d local_static_init.2 ← static int = 77 (inside main)
2000001c D _edata              ← End of .data section
```

#### RAM Region — `.bss` Section (`0x2000001C`)

```
2000001c B _sbss               ← Start of .bss section
2000001c B global_uninit       ← int (zero)
20000020 B global_uninit_array ← int[4] (zero)
20000030 B global_zero         ← int = 0
20000034 B global_zero_array   ← int[3] = {0,0,0}
20000040 b static_uninit       ← static int (zero)
20000044 b static_uninit_arr   ← static int[3] (zero)
20000050 b static_zero         ← static int = 0
20000054 b local_static_zero.1 ← static int = 0 (inside main)
20000058 b local_static_uninit.0 ← static int (inside main)
2000005c B _ebss               ← End of .bss section
```

### Section Headers Verification

```
$ arm-none-eabi-objdump -h main.elf
```

```
Idx Name              Size      VMA       LMA
 0 .vector_table      000000ec  08000000  08000000   ← Flash
 1 .text              00000084  080000ec  080000ec   ← Flash
 2 .rodata            00000010  08000170  08000170   ← Flash
 3 .data              0000001c  20000000  08000180   ← RAM (VMA) / Flash (LMA)
 4 .bss               00000040  2000001c  0800019c   ← RAM
 5 .dynamic_allocations 00000400  2000005c  080001dc ← RAM
```

**Key observation**: The `.data` section has different VMA and LMA:
- **VMA** (Virtual Memory Address) = `0x20000000` (RAM) — where the variable lives at runtime
- **LMA** (Load Memory Address) = `0x08000180` (Flash) — where the init values are stored

The reset handler copies from LMA to VMA at startup.

### `.data` Content in Flash

```
$ arm-none-eabi-objdump -s -j .data main.elf
Contents of section .data:
 20000000 2a000000 ffffffff 41000000 63000000  *.......A...c...
 20000010 0a000000 14000000 4d000000           ........M...
```

Reading the init values (little-endian):
- `2a000000` = `0x0000002A` = **42** (`global_init`)
- `ffffffff` = `0xFFFFFFFF` = **-1** (`global_init_neg`)
- `41000000` = `0x00000041` = **65** = `'A'` (`global_char`)
- `63000000` = `0x00000063` = **99** (`static_init`)
- `0a000000` = `0x0000000A` = **10** (`static_init_arr[0]`)
- `14000000` = `0x00000014` = **20** (`static_init_arr[1]`)
- `4d000000` = `0x0000004D` = **77** (`local_static_init`)

### `.rodata` Content in Flash

```
$ arm-none-eabi-objdump -s -j .rodata main.elf
Contents of section .rodata:
 8000170 64000000 48656c6c 6f205354 4d333200  d...Hello STM32.
```

- `64000000` = `0x00000064` = **100** (`global_const`)
- `48656c6c 6f205354 4d333200` = **"Hello STM32\0"** (`global_const_str`)

### Disassembly of `main()`

```
08000134 <main>:
 8000134:  b480       push    {r7}          // Save frame pointer
 8000136:  b085       sub     sp, #20       // Allocate 20 bytes on stack
 8000138:  af00       add     r7, sp, #0    // Set frame pointer
 800013a:  2301       movs    r3, #1        // local_var = 1
 800013c:  60fb       str     r3, [r7, #12] // Store on stack
 800013e:  235a       movs    r3, #90       // local_char = 'Z' (0x5A)
 8000140:  72fb       strb    r3, [r7, #11] // Store on stack
 8000142:  2307       movs    r3, #7        // local_const = 7
 8000144:  607b       str     r3, [r7, #4]  // Store on stack
 8000146:  4b07       ldr     r3, [pc, #28] // Load &global_init
 8000148:  681b       ldr     r3, [r3, #0]  // Load global_init value
 800014a:  3301       adds    r3, #1        // global_init += 1
 800014c:  4a05       ldr     r2, [pc, #20] // Load &global_init
 800014e:  6013       str     r3, [r2, #0]  // Store back
 8000150:  68fb       ldr     r3, [r7, #12] // Load local_var
 8000152:  3301       adds    r3, #1        // local_var += 1
 8000154:  60fb       str     r3, [r7, #12] // Store back
 8000156:  4b04       ldr     r3, [pc, #16] // Load &local_static_init
 8000158:  681b       ldr     r3, [r3, #0]  // Load value
 800015a:  3301       adds    r3, #1        // += 1
 800015c:  4a02       ldr     r2, [pc, #8]  // Load &local_static_init
 800015e:  6013       str     r3, [r2, #0]  // Store back
 8000160:  e7f1       b.n     8000146       // Loop forever
```

**Observations**:
- `local_var` is on the stack at `[r7, #12]` — allocated with `sub sp, #20`
- `local_char` is on the stack at `[r7, #11]` (byte store with `strb`)
- `local_const` is on the stack at `[r7, #4]` (not optimized away at `-O0`)
- `global_init` is accessed via a pointer stored in the literal pool (`[pc, #28]` → `0x20000000`)
- `local_static_init` is also accessed via pointer (`0x20000018`) — it's in `.data`, not on the stack

---

## Summary: The Complete Boot Sequence

```
Power On / Reset
       │
       ▼
┌─────────────────────────────────────────────┐
│ Cortex-M3 hardware reads vector table       │
│   Word 0 (0x08000000) → SP = 0x20005000    │
│   Word 1 (0x08000004) → PC = reset_handler  │
└─────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────┐
│ reset_handler (core.S)                      │
│   1. Set SP = _estack (0x20005000)          │
│   2. Copy .data: Flash → RAM                │
│      (_sidata → _sdata.._edata)             │
│   3. Zero .bss: RAM                         │
│      (_sbss.._ebss)                         │
│   4. Branch to main                         │
└─────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────┐
│ main() (main.c)                             │
│   - Local variables on stack                │
│   - Global/static vars in .data/.bss        │
│   - Infinite loop                           │
└─────────────────────────────────────────────┘
```

---

## Key Takeaways

1. **`.data` vs `.bss`**: Initialized non-zero variables go to `.data` (init values stored in Flash, copied to RAM at startup). Uninitialized or zero-initialized variables go to `.bss` (zeroed at startup, no Flash storage needed).

2. **`const` goes to Flash**: Global `const` variables are in `.rodata`, which lives in Flash. They're read directly from Flash — no RAM copy needed.

3. **`static` inside a function**: Despite being declared inside a function, `static` variables have static storage duration — they live in `.bss` or `.data`, not on the stack.

4. **Local variables are on the stack**: They're allocated when the function is entered and freed when it returns. They don't appear in `.data` or `.bss`.

5. **The reset handler is essential**: Without it, `.data` variables would contain garbage (Flash content, not init values) and `.bss` variables would contain whatever was in RAM before.

6. **Weak aliases allow C overrides**: You can override any interrupt handler by defining a function with the same name in C. The weak alias in assembly is replaced by the strong symbol from C.
