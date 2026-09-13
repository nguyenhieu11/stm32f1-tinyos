# Part 4: Hardware Interrupts — EXTI Button

Based on [Vivonomicon's Bare Metal STM32 Programming Part 4](https://vivonomicon.com/2018/04/28/bare-metal-stm32-programming-part-4-intro-to-hardware-interrupts/)
adapted for the **STM32F103C8T6** (Blue Pill).

## Overview

Part 3 used **polling** — the main loop continuously reads the button state.
This works but wastes CPU cycles. This part introduces **hardware interrupts**
using the EXTI (External Interrupt/Event Controller), so the CPU only reacts
when the button is actually pressed.

- **PB1** (button) triggers an EXTI interrupt on falling edge
- **PC13** (LED) behavior is controlled by a flag set in the interrupt handler

Behavior:
- **Default**: LED blinks
- **Button press**: toggles to LED on solid
- **Button press again**: toggles back to LED blinking

## Key Difference: STM32F0 vs STM32F1 EXTI

The EXTI peripheral itself is nearly identical between STM32F0 and STM32F1.
The main difference is **how EXTI lines are mapped to GPIO ports**.

### EXTI Line Mapping

| Aspect              | STM32F0 (Tutorial)           | STM32F103 (Our MCU)          |
|---------------------|------------------------------|------------------------------|
| Mapping peripheral  | `SYSCFG`                     | `AFIO`                       |
| Clock enable        | `RCC->APB2ENR \|= SYSCFGEN`  | `RCC->APB2ENR \|= AFIOEN`    |
| Config register     | `SYSCFG->EXTICR[]`           | `AFIO->EXTICR[]`             |
| Register base       | `0x40010000`                 | `0x40010000`                 |

The register layout and bit fields are the same — only the peripheral name
changes. On STM32F1, the AFIO (Alternate Function I/O) peripheral handles
pin remapping and EXTI line selection.

### NVIC IRQ Assignment

| EXTI Lines   | STM32F0 IRQ         | STM32F1 IRQ         |
|--------------|---------------------|---------------------|
| Line 0       | `EXTI0_1_IRQn`      | `EXTI0_IRQn` (IRQ 6)|
| Line 1       | `EXTI0_1_IRQn`      | `EXTI1_IRQn` (IRQ 7)|
| Lines 0–1    | Shared handler      | Individual handlers |
| Lines 2–3    | `EXTI2_3_IRQn`      | `EXTI2_IRQn`, `EXTI3_IRQn` |
| Lines 4–15   | `EXTI4_15_IRQn`     | `EXTI4_IRQn`, `EXTI9_5_IRQn`, `EXTI15_10_IRQn` |

STM32F1 has **individual IRQ numbers** for EXTI lines 0–4, and groups
lines 5–9 and 10–15. The vector table handler names must match exactly.

### Vector Table Handler Names

The handler name in C must match the weak alias in `vector_table.S`:

```c
// STM32F0 (from tutorial):
void EXTI0_1_IRQ_handler(void) { ... }

// STM32F103 (our implementation):
void EXTI1_handler(void) { ... }
```

## How EXTI Works

The EXTI block has one "line" per external interrupt pin. Each line can be
connected to any GPIO port via the AFIO mapping registers.

```
  PB1 (pin)
    │
    ▼
  AFIO->EXTICR[0]  ──maps line 1 to GPIOB──►  EXTI line 1
                                                    │
                                              ┌─────┴─────┐
                                              │           │
                                         EXTI->IMR   EXTI->FTSR
                                         (unmask)    (falling edge)
                                              │           │
                                              └─────┬─────┘
                                                    │
                                                    ▼
                                              EXTI1_handler()
                                              (in NVIC, IRQ 7)
```

When a falling edge is detected on PB1:
1. EXTI sets the pending bit in `EXTI->PR`
2. NVIC routes to `EXTI1_handler()`
3. Handler clears the pending bit and toggles `led_blink`
4. CPU returns to main loop

## Code Walkthrough

### 1. Enable Clocks

```c
RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;   // AFIO clock (for EXTI mapping)
RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;   // GPIOC clock (LED)
RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;   // GPIOB clock (button)
```

The AFIO clock **must** be enabled before accessing `AFIO->EXTICR[]`.
Without it, writes to the mapping registers are silently ignored.

### 2. Configure PB1 as Input with Pull-Up

Same as Part 3:

```c
BUTTON_PORT->CRL &= ~(0xF << (BUTTON_PIN * 4));
BUTTON_PORT->CRL |=  (GPIO_IN_PULL << (BUTTON_PIN * 4));
BUTTON_PORT->ODR |=  (1 << BUTTON_PIN);  // Pull-up
```

### 3. Configure PC13 as Output

Same as Part 3:

```c
LED_PORT->CRH &= ~(0xF << ((LED_PIN - 8) * 4));
LED_PORT->CRH |=  (GPIO_OUT_PP_2MHZ << ((LED_PIN - 8) * 4));
```

### 4. Map EXTI Line 1 to GPIOB

```c
AFIO->EXTICR[(BUTTON_PIN / 4)] &= ~(0xF << ((BUTTON_PIN % 4) * 4));
AFIO->EXTICR[(BUTTON_PIN / 4)] |=  (0x1 << ((BUTTON_PIN % 4) * 4));
```

`AFIO->EXTICR[]` is an array of 4 registers, each controlling 4 EXTI lines:
- `EXTICR[0]` → lines 0–3
- `EXTICR[1]` → lines 4–7
- `EXTICR[2]` → lines 8–11
- `EXTICR[3]` → lines 12–15

Each line uses 4 bits to select the GPIO port:
- `0000` = GPIOA
- `0001` = GPIOB
- `0010` = GPIOC
- ...

For `BUTTON_PIN = 1`: `EXTICR[0]`, bits `[7:4]` → set to `0x1` (GPIOB).

### EXTI Lines: One Port Per Pin Number

There are only **16 EXTI lines** (0–15), one per pin number. Each line can
be connected to **only one GPIO port** at a time:

```
EXTI Line 0  →  pin 0  from ONE port (selected by EXTICR[0] bits [3:0])
EXTI Line 1  →  pin 1  from ONE port (selected by EXTICR[0] bits [7:4])
EXTI Line 2  →  pin 2  from ONE port (selected by EXTICR[0] bits [11:8])
...
EXTI Line 15 →  pin 15 from ONE port (selected by EXTICR[3] bits [15:12])
```

This means **you cannot use PA1 and PB1 with EXTI at the same time**.
EXTI line 1 can point to GPIOA or GPIOB, but not both:

```c
// Selects GPIOB for EXTI line 1
AFIO->EXTICR[0] |= (0x1 << (1 * 4));  // 0001 = GPIOB

// If you change it to GPIOA, PB1 loses EXTI connection
AFIO->EXTICR[0] &= ~(0xF << (1 * 4));
AFIO->EXTICR[0] |= (0x0 << (1 * 4));  // 0000 = GPIOA
```

Only the **last written** port selection takes effect — the 4-bit field
stores one value, not a bitmask.

However, **different pin numbers are independent**. To use two buttons
on EXTI, assign them different pin numbers:

| Button | Pin  | EXTI Line | IRQ            |
|--------|------|-----------|----------------|
| BTN_A  | PA1  | Line 1    | `EXTI1_IRQn`   |
| BTN_B  | PB0  | Line 0    | `EXTI0_IRQn`   |

```
EXTI Line 1 can monitor:         EXTI Line 0 can monitor:
  PA1  ─┐                           PA0  ─┐
  PB1  ─┼──  pick ONE  ──► Line 1   PB0  ─┼──  pick ONE  ──► Line 0
  PC1  ─┤                           PC0  ─┤
  PD1  ─┘                           PD0  ─┘
       (independent, no conflict)
```

In our implementation, only PB1 triggers an interrupt. PB0, PB2, etc.
are on different EXTI lines that we did not enable, so pressing them
has no effect.

### 5. Configure EXTI Trigger

```c
EXTI->IMR  |= (1 << BUTTON_PIN);   // Unmask line 1 (enable interrupt)
EXTI->RTSR &= ~(1 << BUTTON_PIN);  // Disable rising edge
EXTI->FTSR |= (1 << BUTTON_PIN);   // Enable falling edge
```

- **IMR** (Interrupt Mask Register): `1` = interrupt enabled (unmasked)
- **RTSR** (Rising Trigger Selection Register): `1` = trigger on rising edge
- **FTSR** (Falling Trigger Selection Register): `1` = trigger on falling edge

We want falling edge only — when the button is pressed, the pin goes from
high (pull-up) to low (connected to GND).

### 6. Enable NVIC Interrupt

```c
NVIC_SetPriority(EXTI1_IRQn, 0x03);
NVIC_EnableIRQ(EXTI1_IRQn);
```

`EXTI1_IRQn` is IRQ number 7 (defined in `stm32f103x8.h`). Priority `0x03`
is low priority (higher number = lower priority on Cortex-M3).

### 7. Interrupt Handler

```c
void EXTI1_handler(void) {
    if (EXTI->PR & (1 << BUTTON_PIN)) {
        EXTI->PR |= (1 << BUTTON_PIN);   // Clear pending flag
        led_blink = !led_blink;           // Toggle blink mode
    }
}
```

**Important**: The handler name `EXTI1_handler` must match the weak alias
in `vector_table.S` exactly. The vector table entry at position 23 (IRQ 7)
points to this function.

**Clearing the pending bit**: Writing `1` to `EXTI->PR` clears that bit
(this is a write-1-to-clear register). If you forget to clear it, the
interrupt will fire again immediately upon return.

### 8. Main Loop

```c
while (1) {
    if (led_blink) {
        LED_PORT->ODR ^= (1 << LED_PIN);   // Blink — toggle LED
    }
    else {
        LED_PORT->BRR = (1 << LED_PIN);    // Solid — LED on
    }
    for (volatile uint32_t i = 0; i < 500000; i++) {
        __asm__ volatile ("nop");
    }
}
```

The main loop no longer reads the button. It just follows the `led_blink`
flag, which is toggled by the interrupt handler.

## Device Header Additions

`stm32f103x8.h` was extended with:

| Peripheral | Base Address | Key Registers |
|------------|-------------|---------------|
| `AFIO`     | `0x40010000` | `EXTICR[4]` — EXTI line-to-port mapping |
| `EXTI`     | `0x40010400` | `IMR`, `RTSR`, `FTSR`, `PR` |
| `NVIC`     | `0xE000E100` | `ISER[8]`, `ICER[8]`, `IP[240]` |
| `SCB`      | `0xE000ED00` | `SHP[12]` — system handler priority |

Inline helper functions:
- `NVIC_EnableIRQ(IRQn)` — set bit in NVIC ISER
- `NVIC_DisableIRQ(IRQn)` — set bit in NVIC ICER
- `NVIC_SetPriority(IRQn, priority)` — set priority in NVIC IP or SCB SHP

## Files

| File | Purpose |
|------|---------|
| `device_headers/stm32f103x8.h` | Register definitions (RCC, GPIO, AFIO, EXTI, NVIC) |
| `main.h` | Pin assignments and includes |
| `main.c` | EXTI init, interrupt handler, main loop |
| `vector_table.S` | Vector table with EXTI1_handler weak alias |

## Building and Flashing

```bash
make clean && make
st-flash write main.bin 0x08000000
```

## Testing

1. Connect a button between PB1 and GND (internal pull-up is enabled)
2. LED blinks by default (~600ms on/off cycle)
3. Press the button → LED stops blinking and stays on solid
4. Press again → LED resumes blinking
5. Each press toggles between blink and solid modes

## Debugging Tips

If the interrupt doesn't fire:

1. **Check AFIO clock**: `RCC->APB2ENR` bit 0 must be set
2. **Check EXTI mapping**: `AFIO->EXTICR[0]` bits [7:4] should be `0x0001` (GPIOB)
3. **Check EXTI config**: `EXTI->IMR` bit 1 and `EXTI->FTSR` bit 1 should be set
4. **Check NVIC**: `NVIC->ISER[0]` bit 7 should be set (IRQ 7 = EXTI1)
5. **Check handler name**: must be `EXTI1_handler` (not `EXTI1_IRQHandler`)

## Reference

- [RM0008](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) — STM32F10xxx Reference Manual
  - Section 9.4: AFIO registers (EXTI line mapping)
  - Section 10.3: EXTI registers (IMR, RTSR, FTSR, PR)
  - Table 60: Interrupt vector table (EXTI1 = IRQ 7)
- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/) — NVIC registers
