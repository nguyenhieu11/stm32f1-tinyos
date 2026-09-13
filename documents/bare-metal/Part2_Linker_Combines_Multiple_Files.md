# Part 2 Supplement: How the Linker Combines Multiple Files

## Overview

When you build a bare-metal project, multiple source files (`.c`, `.S`) are compiled into separate **object files** (`.o`). The **linker** (`ld`) then combines them into a single ELF executable. This document explains exactly how that process works.

---

## The Three Object Files and Their Sections

Each source file is compiled independently into an object file. Each object file contains **sections** — named chunks of data or code:

```
core.S → core.o              vector_table.S → vector_table.o       main.c → main.o
┌──────────────────┐         ┌──────────────────────┐        ┌──────────────────┐
│ .text            │         │ .vector_table        │        │ .text            │
│  (reset_handler) │         │  (59 table entries)  │        │  (main)          │
│                  │         │                      │        │                  │
│                  │         │ .text.default_       │        │ .data            │
│                  │         │  interrupt_handler   │        │  (global_init=42)│
│                  │         │                      │        │  (static_init=99)│
│ .data (empty)    │         │ .data (empty)        │        │                  │
│ .bss (empty)     │         │ .bss (empty)         │        │ .bss             │
└──────────────────┘         └──────────────────────┘        │  (global_uninit) │
                                                             │                  │
                                                             │ .rodata          │
                                                             │  (global_const)  │
                                                             └──────────────────┘
```

You can inspect the sections in each object file with:

```bash
arm-none-eabi-objdump -h core.o
arm-none-eabi-objdump -h vector_table.o
arm-none-eabi-objdump -h main.o
```

---

## Why `vector_table.o` Has Special Section Names

Notice that `vector_table.o` has `.vector_table` and `.text.default_interrupt_handler` — not the usual `.text`. This comes from **`.section` directives** in the assembly source.

### The `.section` Directive

In `vector_table.S`, there are exactly two `.section` directives:

**Line 31** — the vector table data:

```asm
.section .vector_table,"a",%progbits    ← Create a section named ".vector_table"
vtable:
    .word _estack
    .word reset_handler
    ...
```

**Line 217** — the default interrupt handler:

```asm
.section .text.default_interrupt_handler,"ax",%progbits    ← Create a section named ".text.default_interrupt_handler"
default_interrupt_handler:
    default_interrupt_loop:
      B default_interrupt_loop
```

The `.section` directive tells the assembler: *"Put the following code/data into a section with this specific name, instead of the default `.text`."*

### Section Flags Explained

The flags in quotes define the section's properties:

| Flag | Meaning | Used For |
|---|---|---|
| `a` | **Allocatable** — occupies memory at runtime | Data that must exist in the binary |
| `x` | **Executable** — contains CPU instructions | Code sections |
| `%progbits` | **Contains data** — stored in the file | As opposed to `.bss` which is zero-filled |

So:
- `.vector_table,"a",%progbits` → allocatable data (the table itself)
- `.text.default_interrupt_handler,"ax",%progbits` → allocatable executable code

### Why Not Just Use `.text`?

**For `.vector_table`:**

The vector table **must** be at `0x08000000`. If it were in `.text`, it would be mixed in with code and could end up anywhere. By giving it a custom name, the linker script can place it explicitly:

```ld
.vector_table :              ← Matches ONLY the .vector_table section
{
    KEEP (*(.vector_table))
} >FLASH                     ← At the very start of Flash
```

**For `.text.default_interrupt_handler`:**

This is a **naming convention**. The `.text.` prefix means "this is code, grouped under `.text`." The linker script pattern `*(.text*)` catches it:

```ld
.text :
{
    *(.text)       ← Matches core.o's .text, main.o's .text
    *(.text*)      ← Matches vector_table.o's .text.default_interrupt_handler
} >FLASH
```

So it ends up in the same final `.text` section, but during linking it's kept separate until the merge happens.

### What Happens Without `.section`?

If you removed the `.section` directives, everything would go into the **default section** `.text`:

```asm
vtable:                        ← Would go into .text
    .word _estack
    ...

default_interrupt_handler:     ← Would also go into .text
    B default_interrupt_loop
```

Then the vector table would be mixed in with code, and the linker would have no way to place it at `0x08000000` specifically. The chip would boot into random code instead of reading the vector table.

### How C Files Get Their Section Names

C files don't need `.section` directives — the **compiler** assigns section names automatically based on the variable type:

```c
int global_init = 42;          // Compiler puts this in .data
int global_uninit;             // Compiler puts this in .bss
const int global_const = 100;  // Compiler puts this in .rodata
void main(void) { ... }        // Compiler puts this in .text
```

You can see this in the compiler output:

```bash
arm-none-eabi-objdump -h main.o
# .text     00000038  ...  ← main() function
# .data     0000001c  ...  ← global_init, static_init, etc.
# .bss      00000040  ...  ← global_uninit, static_uninit, etc.
# .rodata   00000010  ...  ← global_const, global_const_str
```

### Summary: Who Creates Section Names?

| Source | How Section Name is Created | Example |
|---|---|---|
| Assembly (`.S`) | Explicit `.section` directive | `.section .vector_table,"a",%progbits` |
| C (`.c`) | Compiler decides based on variable type | `int x = 42;` → `.data` |
| Linker script | Collects and merges by name | `*(.vector_table)`, `*(.text)` |

---

## How the Linker Merges Sections

The linker script's `SECTIONS` block uses **wildcard patterns** to collect sections with the same name from **all** object files:

```ld
SECTIONS
{
  .vector_table :
  {
    KEEP (*(.vector_table))    ← "all .vector_table sections from all files"
  } >FLASH

  .text :
  {
    *(.text)                   ← "all .text sections from all files"
    *(.text*)                  ← "all .text.* sections from all files"
  } >FLASH

  .rodata :
  {
    *(.rodata)                 ← "all .rodata sections from all files"
    *(.rodata*)
  } >FLASH

  .data : AT(_sidata)
  {
    _sdata = .;
    *(.data)                   ← "all .data sections from all files"
    *(.data*)
    _edata = .;
  } >RAM

  .bss :
  {
    _sbss = .;
    *(.bss)                    ← "all .bss sections from all files"
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } >RAM
}
```

The `*` is a wildcard matching any filename. Here's what each pattern collects:

| Linker Script Pattern | Matches From | Combined Result |
|---|---|---|
| `*(.vector_table)` | `vector_table.o` → `.vector_table` | Only `vector_table.o` has this section |
| `*(.text)` | `core.o` → `.text`, `main.o` → `.text` | Both merged into one `.text` |
| `*(.text*)` | `vector_table.o` → `.text.default_interrupt_handler` | Merged into `.text` |
| `*(.rodata)` | `main.o` → `.rodata` | Only `main.o` has this |
| `*(.data)` | `main.o` → `.data` | Only `main.o` has initialized data |
| `*(.bss)` | `main.o` → `.bss` | Only `main.o` has zero-init data |

---

## The `AT(_sidata) > RAM` Pattern

The `.data` section uses two address directives:

```ld
_sidata = .;              ← Records current Flash address (end of .rodata)
.data : AT(_sidata)       ← LMA = Flash (where init values are stored)
{
    _sdata = .;
    *(.data)
    _edata = .;
} >RAM                    ← VMA = RAM (where variables live at runtime)
```

| Directive | Name | Meaning |
|---|---|---|
| `>RAM` | **VMA** | Where variables **live at runtime** (RAM) |
| `AT(_sidata)` | **LMA** | Where init values **are stored in binary** (Flash) |

**Why two addresses?** `.data` variables must be modifiable (→ RAM), but RAM is volatile. So init values are stored in Flash (LMA), then copied to RAM (VMA) at boot by the reset handler.

**Without `AT()`:** VMA = LMA = RAM. Init values would be stored at RAM addresses in the binary, but the binary is flashed to Flash — so variables would contain garbage.

---

## Section Ordering Determines Address Placement

The linker places sections **in the order they appear** in the `SECTIONS` block. Since `MEMORY` defines Flash starting at `0x08000000`:

```
SECTIONS order:          Flash address:
  .vector_table    →     0x08000000  (first)
  .text            →     0x080000EC  (after vector_table)
  .rodata          →     0x08000170  (after text)
  _sidata          →     0x08000180  (after rodata)
```

If you reordered the `SECTIONS` block to put `.text` before `.vector_table`, the code would be at `0x08000000` and the vector table would be elsewhere — and the chip would not boot correctly.

---

## The Final Flash Layout

```
0x08000000 ┌──────────────────────────────────────┐
           │  .vector_table (from vector_table.o) │  236 bytes
           │  Word 0: _estack = 0x20005000        │
           │  Word 1: reset_handler               │
           │  Words 2-58: IRQ handlers            │
0x080000EC ├──────────────────────────────────────┤
           │  .text (from core.o + main.o +       │  132 bytes
           │         vector_table.o)              │
           │  reset_handler (core.o)              │
           │  main (main.o)                       │
           │  default_interrupt_handler (vector_table.o) │
0x08000170 ├──────────────────────────────────────┤
           │  .rodata (from main.o)               │  16 bytes
           │  global_const = 100                  │
           │  global_const_str = "Hello STM32"    │
0x08000180 ├──────────────────────────────────────┤
           │  _sidata (init values for .data)     │  28 bytes
           │  42, -1, 'A', 99, 10, 20, 77         │
0x0800019C └──────────────────────────────────────┘
```

---

## Why `KEEP` is Needed

```ld
KEEP (*(.vector_table))
```

The linker has a garbage collector (`--gc-sections`) that removes sections never referenced by code. The vector table is never "called" — the hardware reads it directly from `0x08000000`. Without `KEEP`, the linker would discard it as unused.

---

## Cross-File Symbol Resolution

Object files reference symbols defined in other files. The linker resolves all of these at link time:

```
core.o references (U = Undefined):
  U _estack        ← defined in linker script
  U _sdata         ← defined in linker script
  U _edata         ← defined in linker script
  U _sidata        ← defined in linker script
  U _sbss          ← defined in linker script
  U _ebss          ← defined in linker script
  U main           ← defined in main.o

vector_table.o references:
  U _estack        ← defined in linker script
  U reset_handler  ← defined in core.o
```

You can see these with:

```bash
arm-none-eabi-nm core.o
#   U _estack    (undefined — will be resolved by linker)
#   U main       (undefined — will be resolved from main.o)
#   T reset_handler  (defined here — T = text/code section)
```

The `U` means "undefined in this file." The linker finds the matching `T` (text), `D` (data), `B` (bss), or linker-script symbol and patches the address.

---

## The Complete Build Pipeline

```
Source Files         Compile                    Object Files          Link                  Final ELF
──────────────     ──────────────             ──────────────       ──────────────        ──────────────
                   arm-none-eabi-gcc
vector_table.S ──→  -x assembler-with-cpp ──→ vector_table.o ──┐
                                                    │          │
                                              .vector_table    │
                                              .text.default_*  │
                                                               ├─→ arm-none-eabi-gcc ──→ main.elf
                   arm-none-eabi-gcc                           │    -T linker_script        │
core.S         ──→  -x assembler-with-cpp ──→ core.o         ──┤                            │
                                                  │            │                            ▼
                                              .text            │                         main.bin
                                                               │                       (objcopy)
                   arm-none-eabi-gcc                           │
main.c         ──→  -c                  ──→ main.o           ──┘
                                                │
                                              .text
                                              .data
                                              .bss
                                              .rodata
```

### What Each Tool Does

| Tool | Input | Output | Purpose |
|---|---|---|---|
| `arm-none-eabi-gcc -x assembler-with-cpp` | `.S` file | `.o` file | Assembles assembly into machine code |
| `arm-none-eabi-gcc -c` | `.c` file | `.o` file | Compiles C into machine code |
| `arm-none-eabi-gcc ... -T script.ld` | `.o` files + `.ld` | `.elf` file | Links all objects, places sections per linker script |
| `arm-none-eabi-objcopy -S -O binary` | `.elf` file | `.bin` file | Strips ELF metadata, produces raw binary for flashing |

---

## Summary

```
                    ┌─────────────────────────────────────────────┐
                    │              Linker Script                  │
                    │                                             │
                    │  MEMORY { FLASH: 0x08000000 }               │
                    │  SECTIONS {                                 │
                    │    .vector_table : { *(.vector_table) }     │
                    │    .text         : { *(.text) *(.text*) }   │
                    │    .rodata       : { *(.rodata) }           │
                    │    .data         : { *(.data) }  >RAM       │
                    │    .bss          : { *(.bss) }   >RAM       │
                    │  }                                          │
                    └─────────┬───────────────────────────────────┘
                              │
          ┌───────────────────┼───────────────────┐
          │                   │                   │
    ┌─────┴─────┐        ┌────┴─────┐        ┌────┴─────┐
    │ core.o    │        │ vector_  │        │ main.o   │
    │ .text     │        │ table.o  │        │ .text    │
    │           │        │ .vector_ │        │ .data    │
    │           │        │ table    │        │ .bss     │
    │           │        │ .text.*  │        │ .rodata  │
    └───────────┘        └──────────┘        └──────────┘
```

**One linker script** controls the placement of sections from **all** object files. The `SECTIONS` block defines the order, and wildcards (`*`) collect matching sections from every file. The result is a single ELF binary with everything at the correct address.

---

## Reserved Keywords vs Conventional Names

| Type | Examples | Who defines them | Case-sensitive? |
|---|---|---|---|
| **Reserved keywords** | `MEMORY`, `ORIGIN`, `LENGTH`, `SECTIONS`, `KEEP`, `ENTRY` | Linker script language | Yes (must be uppercase) |
| **Conventional names** | `.text`, `.data`, `.bss`, `.rodata`, `COMMON` | Toolchain convention | No (but always lowercase) |
| **Custom names** | `.vector_table`, `.my_section`, `.banana` | You (the developer) | No |

- **Reserved keywords** are part of the linker script grammar — the parser recognizes them.
- **Conventional names** are not reserved — the linker accepts any name. These are just standard names the compiler uses consistently.
- **Custom names** are valid too — you just need a matching `.section` directive in your assembly code.

### What Does `ENTRY` Do?

```ld
ENTRY(reset_handler)
```

`ENTRY` writes the address of `reset_handler` into the ELF header's entry point field. This is **metadata for tools** (GDB, debuggers, objcopy) — not for the chip.

The Cortex-M3 boot process is hardwired: it reads SP from `0x08000000` and reset handler from `0x08000004` (the vector table). The chip doesn't read the ELF header and ignores `ENTRY` entirely.
