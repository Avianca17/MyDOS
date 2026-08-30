BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
KERNEL := $(ISO_DIR)/boot/mydoskernel.bin
ISO_IMAGE := $(BUILD_DIR)/mydos.iso

CC := gcc
AS := nasm
LD := ld
GRUB_MKRESCUE ?= grub2-mkrescue
QEMU ?= qemu-system-i386

CFLAGS := -m32 -ffreestanding -fno-pie -fno-stack-protector -Wall -Wextra
CPPFLAGS := -I src -I src/kernel -I src/lib

.PHONY: all iso run clean

all: $(ISO_IMAGE)

iso: $(ISO_IMAGE)

$(BUILD_DIR)/boot.o: src/boot.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf32 $< -o $@

$(BUILD_DIR)/kernel.o: src/kernel/kernel.c src/kernel/kernel.h src/kernel/drivers/vga.h src/lib/string.h src/kernel/drivers/rtc.h src/lib/hardware.h src/kernel/drivers/keyboard.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vga.o: src/kernel/drivers/vga.c src/kernel/kernel.h src/kernel/drivers/vga.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/string.o: src/lib/string.c src/kernel/kernel.h src/lib/string.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/rtc.o: src/kernel/drivers/rtc.c src/kernel/kernel.h src/kernel/drivers/rtc.h src/kernel/drivers/keyboard.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/hardware.o: src/lib/hardware.c src/kernel/kernel.h src/lib/hardware.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: src/kernel/drivers/keyboard.c src/kernel/drivers/keyboard.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(KERNEL): $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/vga.o $(BUILD_DIR)/string.o $(BUILD_DIR)/rtc.o $(BUILD_DIR)/hardware.o $(BUILD_DIR)/keyboard.o linker.ld
	@mkdir -p $(ISO_DIR)/boot
	$(LD) -m elf_i386 -T linker.ld -o $@ $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/vga.o $(BUILD_DIR)/string.o $(BUILD_DIR)/rtc.o $(BUILD_DIR)/hardware.o $(BUILD_DIR)/keyboard.o

$(ISO_DIR)/boot/grub/grub.cfg: iso/boot/grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $< $@

$(ISO_IMAGE): $(KERNEL) $(ISO_DIR)/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ $(ISO_DIR)

run: $(ISO_IMAGE)
	$(QEMU) -cdrom $(ISO_IMAGE)

clean:
	rm -rf $(BUILD_DIR)
