# Makefile for compiling bare-metal ARM Cortex-M3 C/assembly
# Target: STM32F103C8T6 (Blue Pill) with TinyRTOS
TARGET = main

# Define the linker script location and chip architecture.
LD_SCRIPT = STM32F103C8T6.ld
MCU_SPEC = cortex-m3

# Toolchain definitions (ARM bare metal defaults)
TOOLCHAIN = /usr
CC = $(TOOLCHAIN)/bin/arm-none-eabi-gcc
AS = $(TOOLCHAIN)/bin/arm-none-eabi-as
LD = $(TOOLCHAIN)/bin/arm-none-eabi-ld
OC = $(TOOLCHAIN)/bin/arm-none-eabi-objcopy
OD = $(TOOLCHAIN)/bin/arm-none-eabi-objdump
OS = $(TOOLCHAIN)/bin/arm-none-eabi-size

# Assembly directives.
ASFLAGS += -c
ASFLAGS += -O0
ASFLAGS += -mcpu=$(MCU_SPEC)
ASFLAGS += -mthumb
ASFLAGS += -Wall
ASFLAGS += -fmessage-length=0

# C compilation directives.
CFLAGS += -mcpu=$(MCU_SPEC)
CFLAGS += -mthumb
CFLAGS += -Wall
CFLAGS += -g
CFLAGS += -O1
CFLAGS += -fmessage-length=0
CFLAGS += --specs=nosys.specs
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections

# Linker directives.
LSCRIPT = ./$(LD_SCRIPT)
LFLAGS += -mcpu=$(MCU_SPEC)
LFLAGS += -mthumb
LFLAGS += -Wall
LFLAGS += --specs=nosys.specs
LFLAGS += -nostdlib
LFLAGS += -lgcc
LFLAGS += -T$(LSCRIPT)
LFLAGS += -Wl,--gc-sections

# Include paths.
INCLUDE  = -I./device_headers
INCLUDE += -I./include
INCLUDE += -I./src/bsp/blue_pill

# Assembly source files.
AS_SRC  = ./core.S
AS_SRC += ./vector_table.S

# C source files — kernel
C_SRC   = ./main.c
C_SRC  += ./src/kernel/kernel.c
C_SRC  += ./src/kernel/task.c
C_SRC  += ./src/kernel/scheduler.c
C_SRC  += ./src/kernel/port.c
C_SRC  += ./src/kernel/fault.c
C_SRC  += ./src/kernel/semaphore.c
C_SRC  += ./src/kernel/mutex.c
C_SRC  += ./src/kernel/queue.c
# C source files — BSP
C_SRC  += ./src/bsp/blue_pill/clock.c
C_SRC  += ./src/bsp/blue_pill/led.c

# Object files
OBJS  = $(AS_SRC:.S=.o)
OBJS += $(C_SRC:.c=.o)

.PHONY: all
all: $(TARGET).bin

# Pattern rule: .S -> .o
%.o: %.S
	$(CC) -x assembler-with-cpp $(ASFLAGS) $< -o $@

# Pattern rule: .c -> .o
%.o: %.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

# Link all object files into the final ELF.
$(TARGET).elf: $(OBJS)
	$(CC) $^ $(LFLAGS) -o $@

# Create a .bin file from the ELF and print size info.
$(TARGET).bin: $(TARGET).elf
	$(OC) -S -O binary $< $@
	$(OS) $<

.PHONY: clean
clean:
	rm -f $(OBJS)
	rm -f $(TARGET).elf
	rm -f $(TARGET).bin
