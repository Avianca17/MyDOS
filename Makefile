BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
KERNEL := $(ISO_DIR)/boot/mydoskernel.bin
ISO_IMAGE := $(BUILD_DIR)/MyDOS.iso

CC := gcc
AS := nasm
LD := ld
GRUB_MKRESCUE ?= grub-mkrescue
QEMU ?= qemu-system-i386

CFLAGS := -m32 -ffreestanding -fno-pie -fno-stack-protector -Wall -Wextra
CPPFLAGS := -I src -I src/kernel -I src/lib

C_SOURCES := $(shell find src -type f -name '*.c' | sort)
ASM_SOURCES := $(shell find src -type f -name '*.asm' | sort)
LINKER_SCRIPT := $(firstword $(shell find . -type f -name '*.ld' | sort))

C_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst src/%.asm,$(BUILD_DIR)/%.o,$(ASM_SOURCES))

.PHONY: all iso run clean

all: $(ISO_IMAGE)

iso: $(ISO_IMAGE)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

$(KERNEL): $(ASM_OBJECTS) $(C_OBJECTS) $(LINKER_SCRIPT)
	@mkdir -p $(ISO_DIR)/boot
	$(LD) -m elf_i386 -T $(LINKER_SCRIPT) -o $@ $(ASM_OBJECTS) $(C_OBJECTS)

$(ISO_DIR)/boot/grub/grub.cfg: iso/boot/grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $< $@

$(ISO_IMAGE): $(KERNEL) $(ISO_DIR)/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ $(ISO_DIR)

run: $(ISO_IMAGE)
	$(QEMU) -cdrom $(ISO_IMAGE)

clean:
	rm -rf $(BUILD_DIR)
