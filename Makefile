.DEFAULT_GOAL := all

BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
KERNEL := $(ISO_DIR)/mydos/mydoskernel.bin
ISO_IMAGE := $(BUILD_DIR)/MyDOS.iso

CC := gcc
AS := nasm
LD := ld
XORRISO ?= xorriso
TAR ?= tar
DD ?= dd
QEMU ?= qemu-system-i386
QEMU_AUDIO_ARGS ?= -audiodev pa,id=pcspk -machine pc,pcspk-audiodev=pcspk
QEMU_STORAGE_ARGS ?=
QEMU_NET_ARGS ?= -nic user,model=e1000
LIMINE ?= limine
LIMINE_DATA_DIR ?= $(shell $(LIMINE) --print-datadir)

CFLAGS := -m32 -ffreestanding -fno-pie -fno-stack-protector -Wall -Wextra -MMD -MP -mno-sse -mno-sse2 -mfpmath=387
CPPFLAGS := -I src -I src/kernel -I src/lib -I src/fs/fatfs/source -I src/programs/myeditor

DOOM_DIR := src/programs/doom
DOOM_CPPFLAGS := -I $(DOOM_DIR)/port -I $(DOOM_DIR) -DNORMALUNIX -include alloca.h

LIMINE_BOOT_FILES := $(addprefix $(LIMINE_DATA_DIR)/, \
	limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin \
	BOOTX64.EFI BOOTIA32.EFI)

C_SOURCES := $(shell find src -type f -name '*.c' ! -path 'src/fs/littlefs/*' ! -path 'src/fs/fatfs/*' | sort) \
     src/fs/fatfs/source/ff.c src/fs/fatfs/source/diskio.c src/fs/fatfs/source/ffunicode.c
ASM_SOURCES := $(shell find src -type f -name '*.asm' | sort)
LINKER_SCRIPT := $(firstword $(shell find . -type f -name '*.ld' | sort))

C_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst src/%.asm,$(BUILD_DIR)/%.o,$(ASM_SOURCES))

-include $(C_OBJECTS:.o=.d)

.PHONY: all iso run limine-iso run-limine clean

all: $(ISO_IMAGE)

iso: $(ISO_IMAGE)

limine-iso: $(ISO_IMAGE)

$(BUILD_DIR)/programs/doom/port/%.o: $(DOOM_DIR)/port/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(DOOM_CPPFLAGS) $(CFLAGS) -std=gnu11 -c $< -o $@

$(BUILD_DIR)/programs/doom/%.o: $(DOOM_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(DOOM_CPPFLAGS) $(CFLAGS) -std=gnu89 -w -c $< -o $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

$(KERNEL): $(ASM_OBJECTS) $(C_OBJECTS) $(LINKER_SCRIPT)
	@mkdir -p $(dir $@)
	$(LD) -m elf_i386 -T $(LINKER_SCRIPT) -o $@ $(ASM_OBJECTS) $(C_OBJECTS)

$(ISO_IMAGE): Makefile $(KERNEL) iso/limine.conf iso/limine-installed.conf \
	src/kernel/drivers/Spleen-LICENSE.txt $(LIMINE_BOOT_FILES)
	@set -eu; \
	tmpdir=$$(mktemp -d "$(BUILD_DIR)/limine.XXXXXX"); \
	trap 'chmod -R u+w "$$tmpdir"; rm -rf "$$tmpdir"' EXIT; \
	root="$$tmpdir/iso"; package="$$tmpdir/package"; \
	mkdir -p "$$root/boot/limine" "$$root/EFI/BOOT" "$$root/mydos" \
		"$$package/boot/limine" "$$package/EFI/BOOT"; \
	cp "$(KERNEL)" "$$root/mydos/mydoskernel.bin"; \
	cp src/kernel/drivers/Spleen-LICENSE.txt "$$root/Spleen-LICENSE.txt"; \
	cp iso/limine.conf "$$root/limine.conf"; \
	cp "$(LIMINE_DATA_DIR)/limine-bios.sys" "$$root/boot/limine/limine-bios.sys"; \
	cp "$(LIMINE_DATA_DIR)/limine-bios-cd.bin" "$$root/boot/limine/limine-bios-cd.bin"; \
	cp "$(LIMINE_DATA_DIR)/limine-uefi-cd.bin" "$$root/boot/limine/limine-uefi-cd.bin"; \
	cp "$(LIMINE_DATA_DIR)/BOOTX64.EFI" "$$root/EFI/BOOT/BOOTX64.EFI"; \
	cp "$(LIMINE_DATA_DIR)/BOOTIA32.EFI" "$$root/EFI/BOOT/BOOTIA32.EFI"; \
	$(XORRISO) -as mkisofs -R -r -J \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		-o "$$tmpdir/template.iso" "$$root"; \
	$(LIMINE) bios-install "$$tmpdir/template.iso"; \
	$(DD) if="$$tmpdir/template.iso" of="$$package/boot/limine/limine-bios-mbr.bin" \
		bs=512 count=1 status=none; \
	$(DD) if="$$tmpdir/template.iso" of="$$package/boot/limine/limine-bios-stage2.bin" \
		bs=512 skip=8 count=55 status=none; \
	mkdir -p "$$package/boot/limine" "$$package/EFI/BOOT"; \
	cp "$(KERNEL)" "$$package/boot/limine/mydoskernel.bin"; \
	cp src/kernel/drivers/Spleen-LICENSE.txt "$$package/Spleen-LICENSE.txt"; \
	cp iso/limine-installed.conf "$$package/limine.conf"; \
	cp iso/limine-installed.conf "$$package/boot/limine/limine.conf"; \
	cp "$(LIMINE_DATA_DIR)/limine-bios.sys" "$$package/boot/limine/limine-bios.sys"; \
	cp "$(LIMINE_DATA_DIR)/BOOTX64.EFI" "$$package/EFI/BOOT/BOOTX64.EFI"; \
	cp "$(LIMINE_DATA_DIR)/BOOTIA32.EFI" "$$package/EFI/BOOT/BOOTIA32.EFI"; \
	$(TAR) -cf "$$root/mydos/install.tar" -C "$$package" \
		Spleen-LICENSE.txt limine.conf boot/limine/limine.conf boot/limine/limine-bios.sys \
		boot/limine/limine-bios-mbr.bin boot/limine/limine-bios-stage2.bin \
		boot/limine/mydoskernel.bin EFI/BOOT/BOOTX64.EFI EFI/BOOT/BOOTIA32.EFI; \
	$(XORRISO) -as mkisofs -R -r -J \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		-o "$@" "$$root"; \
	$(LIMINE) bios-install "$@"

run: $(ISO_IMAGE)
	$(QEMU) -cdrom $(ISO_IMAGE) $(QEMU_AUDIO_ARGS) $(QEMU_STORAGE_ARGS) $(QEMU_NET_ARGS)

run-limine: run

clean:
	rm -rf $(BUILD_DIR)
