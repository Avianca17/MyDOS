.DEFAULT_GOAL := all

BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
KERNEL := $(ISO_DIR)/mydos/mydoskernel.bin
ISO_IMAGE := $(BUILD_DIR)/MyDOS.iso

CC := gcc
AS := nasm
LD := ld
GRUB_MKRESCUE ?= grub-mkrescue
GRUB_MKRESCUE_FLAGS ?= --install-modules='multiboot normal boot iso9660 configfile' --fonts='' --locales='' --themes=''
GRUB_BIOS_DIR ?= /usr/lib/grub/i386-pc
XORRISO ?= xorriso
QEMU ?= qemu-system-i386
QEMU_AUDIO_ARGS ?= -audiodev pa,id=pcspk -machine pc,pcspk-audiodev=pcspk
QEMU_STORAGE_ARGS ?=

CFLAGS := -m32 -ffreestanding -fno-pie -fno-stack-protector -Wall -Wextra -MMD -MP
CPPFLAGS := -I src -I src/kernel -I src/lib -I src/fs/fatfs/source -I src/programs/myeditor

C_SOURCES := $(shell find src -type f -name '*.c' ! -path 'src/fs/littlefs/*' ! -path 'src/fs/fatfs/*' | sort) \
     src/fs/fatfs/source/ff.c src/fs/fatfs/source/diskio.c src/fs/fatfs/source/ffunicode.c
ASM_SOURCES := $(shell find src -type f -name '*.asm' | sort)
LINKER_SCRIPT := $(firstword $(shell find . -type f -name '*.ld' | sort))

C_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst src/%.asm,$(BUILD_DIR)/%.o,$(ASM_SOURCES))

-include $(C_OBJECTS:.o=.d)

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
	@mkdir -p $(dir $@)
	$(LD) -m elf_i386 -T $(LINKER_SCRIPT) -o $@ $(ASM_OBJECTS) $(C_OBJECTS)

$(ISO_DIR)/boot/grub/grub.cfg: iso/boot/grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $< $@

$(ISO_IMAGE): $(KERNEL) $(ISO_DIR)/boot/grub/grub.cfg
	@set -eu; \
	tmpdir=$$(mktemp -d); \
	trap 'chmod -R u+w "$$tmpdir"; rm -rf "$$tmpdir"' EXIT; \
	$(GRUB_MKRESCUE) $(GRUB_MKRESCUE_FLAGS) -o "$$tmpdir/grub.iso" $(ISO_DIR); \
	mkdir -p "$$tmpdir/tree"; \
	cp -a $(ISO_DIR)/. "$$tmpdir/tree/"; \
	$(XORRISO) -osirrox on -indev "$$tmpdir/grub.iso" \
		-extract /boot/grub/i386-pc "$$tmpdir/tree/boot/grub/i386-pc"; \
	$(XORRISO) -as mkisofs -o "$@" -r -J -V MYDOS \
		-b boot/grub/i386-pc/eltorito.img -c boot.catalog \
		-no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info \
		--grub2-mbr "$(GRUB_BIOS_DIR)/boot_hybrid.img" "$$tmpdir/tree"

run: $(ISO_IMAGE)
	$(QEMU) -cdrom $(ISO_IMAGE) $(QEMU_AUDIO_ARGS) $(QEMU_STORAGE_ARGS)

clean:
	rm -rf $(BUILD_DIR)