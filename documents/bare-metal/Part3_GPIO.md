# Part 3: GPIO — LEDs and Buttons

Based on [Vivonomicon's Bare Metal STM32 Programming Part 3](https://vivonomicon.com/2018/04/22/bare-metal-stm32-programming-part-3-leds-and-buttons/)
adapted for the **STM32F103C8T6** (Blue Pill).

## Overview

This part introduces GPIO (General-Purpose Input/Output) — the most basic
peripheral for interacting with the outside world. We configure:

- **PC13** as a push-pull output (onboard LED, active low)
- **PB1** as an input with pull-up (button, active low)

Behavior:
- **Button not pressed**: LED blinks (toggle with delay)
- **Button pressed**: LED stays on solid

## Key Difference: STM32F1 vs STM32F0 GPIO Registers

The STM32F0 (from the tutorial) and STM32F103 use **completely different**
GPIO register layouts. This is the most important thing to understand.

### STM32F0 (Tutorial's MCU)

Uses separate registers for each configuration aspect:

| Register  | Bits/pin | Purpose                    |
|-----------|----------|----------------------------|
| `MODER`   | 2        | Mode (in/out/AF/analog)    |
| `OTYPER`  | 1        | Output type (PP/OD)        |
| `OSPEEDR` | 2        | Output speed               |
| `PUPDR`   | 2        | Pull-up/pull-down          |

### STM32F103 (Our MCU)

Uses two registers with 4 bits per pin combining all configuration:

| Register | Pins   | Purpose                           |
|----------|--------|-----------------------------------|
| `CRL`    | 0–7    | Configuration (CNF + MODE)        |
| `CRH`    | 8–15   | Configuration (CNF + MODE)        |

Each pin's 4 bits are split:

```
Bit 3  Bit 2  Bit 1  Bit 0
 CNF1   CNF0  MODE1  MODE0
```

**MODE[1:0]** — Selects speed (only matters for output):
- `00` = Input mode
- `01` = Output, 10 MHz max
- `10` = Output, 2 MHz max
- `11` = Output, 50 MHz max

**CNF[1:0]** — Depends on mode:

In **input** mode (MODE=00):
- `00` = Analog input
- `01` = Floating input (reset default)
- `10` = Input with pull-up/pull-down
- `11` = Reserved

In **output** mode (MODE≠00):
- `00` = Push-pull output
- `01` = Open-drain output
- `10` = Alternate function push-pull
- `11` = Alternate function open-drain

### Clock Enable

Another key difference: GPIO clocks are on **APB2** (not AHB):

```c
// STM32F0:
RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

// STM32F103:
RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;  // GPIOC (LED)
RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;  // GPIOB (button)
```

## Code Walkthrough

### 1. Enable Clocks

```c
RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;  // GPIOC clock (LED)
RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;  // GPIOB clock (button)
```

Without enabling the clock, the GPIO registers are inaccessible and the
peripheral won't work.

### 2. Configure PB1 as Input with Pull-Up (Button)

```c
// Clear bits [7:4] in CRL (pin 1)
BUTTON_PORT->CRL &= ~(0xF << (BUTTON_PIN * 4));
// Set CNF=10, MODE=00 → input with pull-up/pull-down
BUTTON_PORT->CRL |=  (GPIO_IN_PULL << (BUTTON_PIN * 4));
// ODR=1 selects pull-up (ODR=0 would select pull-down)
BUTTON_PORT->ODR |=  (1 << BUTTON_PIN);
```

The internal pull-up is ~47KΩ. When the button is not pressed, the pin reads
high (1). When pressed, the pin connects to GND and reads low (0).

### 3. Configure PC13 as Push-Pull Output (LED)

```c
// Clear bits [23:20] in CRH (pin 13 → offset 5, 5*4=20)
LED_PORT->CRH &= ~(0xF << ((LED_PIN - 8) * 4));
// Set CNF=00, MODE=10 → push-pull output at 2 MHz
LED_PORT->CRH |=  (GPIO_OUT_PP_2MHZ << ((LED_PIN - 8) * 4));
```

Note: For pins 8–15, we use `CRH` and the bit offset is `(pin - 8) * 4`.

### 4. Main Loop — Blink or Solid

```c
while (1) {
    if (~BUTTON_PORT->IDR & (1 << BUTTON_PIN)) {
        // Button pressed — LED on solid (BRR clears bit → pin low → LED on)
        LED_PORT->BRR = (1 << LED_PIN);
    }
    else {
        // Button not pressed — blink LED (toggle each iteration)
        LED_PORT->ODR ^= (1 << LED_PIN);
    }
    // Delay for visible blink rate (~600ms at 8MHz HSI)
    for (volatile uint32_t i = 0; i < 500000; i++) {
        __asm__ volatile ("nop");
    }
}
```

The logic:
1. Read `IDR`, invert it (so 1 = pressed, since pull-up means idle high)
2. If button pressed → force LED on with `BRR` (set pin low, active-low LED)
3. If button not pressed → toggle LED with `ODR ^=` (creates blink)
4. Delay loop makes the blink visible (~600ms per half-cycle)

`BRR` (Bit Reset Register) is a handy STM32F1 register — writing a 1 to a
bit clears that ODR bit directly, without needing a read-modify-write.

### 5. LED Active-Low Note

The Blue Pill's PC13 LED is **active low**:
- `ODR bit = 1` → pin high → LED **off**
- `ODR bit = 0` → pin low → LED **on**

We use `BRR` to turn the LED on (clears ODR bit → pin low), and
`ODR ^= ...` to toggle it for the blink effect.

## Files

| File | Purpose |
|------|---------|
| `device_headers/stm32f103x8.h` | Minimal register definitions (RCC, GPIO) |
| `main.h` | Pin assignments and includes |
| `main.c` | GPIO init + button/LED logic |

## Building and Flashing

```bash
make clean && make
st-flash write main.bin 0x08000000
```

## Testing

1. Connect a button between PB1 and GND (the internal pull-up is enabled)
2. LED blinks by default (~600ms on/off cycle)
3. Press and hold the button → LED stays on solid
4. Release → LED resumes blinking

## Reference

- [RM0008](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) — STM32F10xxx Reference Manual
  - Section 7.3.7: RCC_APB2ENR (clock enable)
  - Section 9.2: GPIO registers (CRL/CRH)
