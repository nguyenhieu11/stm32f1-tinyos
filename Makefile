# Makefile for compiling bare-metal ARM Cortex-M3 C/assembly
# Target: STM32F103C8T6 (Blue Pill)
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
# += appends to the variable (e.g. ASFLAGS grows with each line).
ASFLAGS += -c
ASFLAGS += -O0
ASFLAGS += -mcpu=$(MCU_SPEC)
ASFLAGS += -mthumb
ASFLAGS += -Wall
# (Set error messages to appear on a single line.)
ASFLAGS += -fmessage-length=0

# C compilation directives.
CFLAGS += -mcpu=$(MCU_SPEC)
CFLAGS += -mthumb
CFLAGS += -Wall
CFLAGS += -g
# (Set error messages to appear on a single line.)
CFLAGS += -fmessage-length=0
# (Set system to ignore semihosted junk)
CFLAGS += --specs=nosys.specs

# Linker directives.
LSCRIPT = ./$(LD_SCRIPT)
LFLAGS += -mcpu=$(MCU_SPEC)
LFLAGS += -mthumb
LFLAGS += -Wall
LFLAGS += --specs=nosys.specs
LFLAGS += -nostdlib
LFLAGS += -lgcc
LFLAGS += -T$(LSCRIPT)

# Include paths.
INCLUDE  = -I./device_headers
INCLUDE += -I./include

# Source files.
AS_SRC  = ./core.S
AS_SRC += ./vector_table.S
C_SRC   = ./main.c
C_SRC  += ./src/kernel/kernel.c
C_SRC  += ./src/kernel/task.c
C_SRC  += ./src/kernel/port.c
C_SRC  += ./src/kernel/scheduler.c
C_SRC  += ./src/kernel/fault.c

# Substitution reference: replaces .S with .o in AS_SRC.
#   $(AS_SRC:.S=.o)  =>  ./core.o ./vector_table.o
OBJS  = $(AS_SRC:.S=.o)
OBJS += $(C_SRC:.c=.o)

# .PHONY tells make that "all" and "clean" are not real files.
# Without this, if a file named "all" or "clean" existed in the
# directory, make would think the target is already up to date.
.PHONY: all
all: $(TARGET).bin

# Pattern rule: builds any .o from its matching .S file.
#   %.o  matches any target ending in .o  (e.g. core.o)
#   %.S  matches the prerequisite ending in .S (e.g. core.S)
#
# Automatic variables used in the recipe:
#   $<  = first prerequisite       => core.S
#   $@  = target name              => core.o
%.o: %.S
	$(CC) -x assembler-with-cpp $(ASFLAGS) $< -o $@

# Pattern rule: builds any .o from its matching .c file.
%.o: %.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

# Link all object files into the final ELF.
#   $^  = all prerequisites        => core.o vector_table.o main.o
#   $@  = target name              => main.elf
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
