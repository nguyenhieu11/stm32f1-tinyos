# Bare Metal STM32F103C8T6 (Blue Pill) — Hello ARM

A minimal bare-metal program for the STM32F103C8T6 "Blue Pill" board.
Based on [Vivonomicon's Bare Metal STM32 Programming Part 1](https://vivonomicon.com/2018/04/02/bare-metal-stm32-programming-part-1-hello-arm/),
adapted from the STM32F0 (Cortex-M0) to the STM32F1 (Cortex-M3).

## What This Does

The program does the absolute minimum to prove the chip is alive:

1. Sets the stack pointer to the end of SRAM
2. Loads `0xDEADBEEF` into register `r7` (a sentinel you can spot in the debugger)
3. Counts up in `r0` in an infinite loop

That's it — no clocks, no GPIO, no peripherals. Just the CPU running instructions.

## Prerequisites

Install the ARM bare-metal toolchain and an ST-Link programmer:

```bash
sudo apt install gcc-arm-none-eabi gdb-multiarch stlink-tools
```

> **Note:** We use `gdb-multiarch` instead of `arm-none-eabi-gdb` — it's the same GDB
> but supports multiple architectures in one package. On some distros `arm-none-eabi-gdb`
> is not available or is harder to install.

## Build

```bash
make
```

This produces `main.elf`. To clean build artifacts:

```bash
make clean
```

To inspect the resulting symbols:

```bash
arm-none-eabi-nm main.elf
```

Expected output:

```
20005000 A _estack
08000010 t main_loop
08000008 T reset_handler
08000000 T vtable
```

Each line has three columns: `address type name`.

**Address** — where the symbol lives in memory:

| Symbol | Address | Meaning |
|---|---|---|
| `vtable` | `0x08000000` | Start of Flash — the CPU reads the vector table from here on reset |
| `reset_handler` | `0x08000008` | 8 bytes into Flash, right after the 2-word vector table |
| `main_loop` | `0x08000010` | The loop label inside `reset_handler` |
| `_estack` | `0x20005000` | End of 20KB SRAM — the initial stack pointer value |

**Type** — symbol kind (from `nm`):

| Letter | Meaning |
|---|---|
| `T` | Text (code) in a **global** section — `vtable` and `reset_handler` are `.global` |
| `t` | Text (code) in a **local** section — `main_loop` is not `.global`, so it's local |
| `A` | Absolute value — `_estack` is a constant defined in the linker script, not something stored in Flash |

The `A` on `_estack` is worth noting: it doesn't occupy any Flash bytes. The linker just substitutes `0x20005000` wherever `_estack` is referenced (the `.word _estack` in the vector table and the `LDR r0, =_estack` in the reset handler).

## Flash & Debug

You need an **ST-Link V2** programmer/debugger connected to the Blue Pill:

| ST-Link Pin | Blue Pill Pin |
|-------------|---------------|
| SWDIO       | SWDIO (PA13)  |
| SWCLK       | SWCLK (PA14)  |
| GND         | GND           |
| 3.3V        | 3.3V (optional, if not powering via USB) |

**Terminal 1** — start the GDB server:

```bash
st-util
```

```
2026-08-30T21:43:42 WARN common.c: NRST is not connected
2026-08-30T21:43:42 INFO common.c: F1xx Medium-density: 20 KiB SRAM, 128 KiB flash in at least 1 KiB pages.
2026-08-30T21:43:42 INFO gdb-server.c: Listening at *:4242...
```

What `st-util` does step by step:

1. **Connects to ST-Link** via USB and opens the SWD (Serial Wire Debug) interface to the chip
2. **Warns about NRST** — the reset pin is not wired between ST-Link and Blue Pill (normal if you only connected SWDIO/SWCLK/GND)
3. **Reads the chip ID** and identifies it as F1xx Medium-density (STM32F103C8T6: 20KB SRAM, 64KB Flash — the "128 KiB" is the flash page size group, not your exact chip)
4. **Listens on port 4242** — waiting for GDB to connect

**Terminal 2** — connect GDB and load the program:

```bash
gdb-multiarch main.elf
```

Inside GDB:

```gdb
(gdb) target extended-remote :4242
```

GDB output:

```
Remote debugging using :4242
0x08000012 in reset_handler ()
```

st-util output (Terminal 1):

```
2026-08-30T21:44:10 WARN common.c: NRST is not connected
2026-08-30T21:44:10 INFO common.c: F1xx Medium-density: 20 KiB SRAM, 128 KiB flash in at least 1 KiB pages.
2026-08-30T21:44:10 INFO gdb-server.c: Found 6 hw breakpoint registers
2026-08-30T21:44:10 INFO gdb-server.c: GDB connected
```

What happens:

1. **GDB connects** to `st-util` on port 4242
2. **st-util halts the CPU** and reads the current PC (`0x08000012` = inside `reset_handler`) — the chip was already running from a previous flash
3. **Reports 6 hardware breakpoints** — the Cortex-M3 has 6 breakpoint registers for debugging
4. **GDB shows the current location** — `reset_handler+10` (the `main_loop` label, since the CPU was looping there)

```gdb
(gdb) load
```

GDB output:

```
Loading section .text, size 0x1c lma 0x8000000
Start address 0x08000000, load size 28
Transfer rate: 180 bytes/sec, 28 bytes/write.
```

st-util output (Terminal 1):

```
2026-08-30T21:44:15 WARN common.c: NRST is not connected
2026-08-30T21:44:15 INFO common.c: F1xx Medium-density: 20 KiB SRAM, 128 KiB flash in at least 1 KiB pages.
2026-08-30T21:44:15 INFO gdb-server.c: flash_erase: block 08000000 -> 0400
2026-08-30T21:44:15 INFO gdb-server.c: flash_erase: page 08000000
2026-08-30T21:44:15 INFO common.c: Starting Flash write for VL/F0/F3/F1_XL
2026-08-30T21:44:15 INFO flash_loader.c: Successfully loaded flash loader in sram
2026-08-30T21:44:15 INFO flash_loader.c: Clear DFSR
2026-08-30T21:44:15 INFO gdb-server.c: flash_do: block 08000000 -> 0400
2026-08-30T21:44:15 INFO gdb-server.c: flash_do: page 08000000
```

What happens step by step:

1. **`flash_erase: page 08000000`** — erases the Flash page at `0x08000000` (Flash must be erased before writing; erasing sets all bits to `0xFF`)
2. **`Successfully loaded flash loader in sram`** — uploads a small flash-writing program into the chip's SRAM. The STM32 can't write to Flash while executing from Flash, so the loader runs from RAM instead
3. **`Clear DFSR`** — clears the Debug Fault Status Register before programming
4. **`flash_do: block 08000000 -> 0400`** — writes 28 bytes (`0x0400` is not a size, it's part of the block range) to Flash starting at `0x08000000`
5. **Program is now in Flash** — persists after power off/on

```gdb
(gdb) continue
```

```
Continuing.
```

The program is now running. Press `Ctrl+C` to pause it:

```
^C
Program received signal SIGTRAP, Trace/breakpoint trap.
0x08000012 in reset_handler ()
```

Then inspect registers:

```gdb
(gdb) info registers
```

```
r0             0x7acc74            8047732
r1             0x0                 0
r2             0x0                 0
r3             0x0                 0
r4             0x0                 0
r5             0x0                 0
r6             0x0                 0
r7             0xdeadbeef          -559038737
r8             0x0                 0
r9             0x0                 0
r10            0x0                 0
r11            0x0                 0
r12            0x0                 0
sp             0x20005000          0x20005000
lr             0xffffffff          -1
pc             0x8000012           0x8000012 <reset_handler+10>
xpsr           0x1000000           16777216
msp            0x20005000          0x20005000
psp            0x0                 0x0
control        0x0                 0 '\000'
faultmask      0x0                 0 '\000'
basepri        0x0                 0 '\000'
primask        0x0                 0 '\000'
fpscr          0x0                 0
```

Key things to look for:

| Register | Value | Meaning |
|---|---|---|
| `r7` | `0xdeadbeef` | Our sentinel value — proves the reset handler ran |
| `r0` | `0x7acc74` (varies) | The counter — increments each loop iteration |
| `sp` / `msp` | `0x20005000` | Stack pointer at end of SRAM, as defined in the linker script |
| `pc` | `0x8000012` | Currently executing inside `reset_handler` (at `main_loop`) |

## Project Structure

```
.
├── core.S              # Startup code: vector table + reset handler
├── STM32F103C8T6.ld    # Linker script (Flash/RAM layout)
├── Makefile            # Build system
└── README.md           # This file
```

## Key Differences from the STM32F0 Example

| | STM32F0 (original) | STM32F103C8T6 (this) |
|---|---|---|
| Core | Cortex-M0 | Cortex-M3 |
| `.cpu` directive | `cortex-m0` | `cortex-m3` |
| Flash | 32KB @ `0x08000000` | 64KB @ `0x08000000` |
| SRAM | 4KB @ `0x20000000` | 20KB @ `0x20000000` |
| Stack top (`_estack`) | `0x20001000` | `0x20005000` |

## License

MIT
