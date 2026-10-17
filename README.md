# MyDOS

MyDOS is a small 32-bit x86 hobby operating system written in C and assembly. It boots with GRUB and runs in QEMU.

## What it includes

- Bare-metal kernel
- FAT32 filesystem support
- ATA/IDE and AHCI SATA support
- Simple shell
- Basic text editor
- Kernel Panics (NEW)

## Commands

Shell:
- help [command]
- beep
- clear [-dl|--draw-logo]
- echo [-c <color>|--color <color>] <text>
- change-theme <default|matrix|ocean|sunset>
- loadkeys <us|de|fr>
- date [-t|--time] [-d|--date] [-12|--12h]
- uptime [-p|--pretty] [-s|--seconds]
- fetch
- shlogo

Filesystem:
- ls
- lsdrv
- mkfile <filename>
- mkdir <dirname>
- rm [-r] <path>
- cd /PATH/TO/DIRECTORY
- cat <filename>
- myeditor
- pwd

System:
- rbt
- shtdwn
- fault <zero|ud>

## Requirements

- gcc
- nasm
- ld
- grub-mkrescue
- xorriso
- qemu-system-i386

## Build

```sh
make
```

If GRUB is not installed in the default BIOS path, set `GRUB_BIOS_DIR` before building.

## Run

```sh
make run
```

## Optional AHCI disk

```sh
qemu-img create -f raw sata.img 128M
make run QEMU_STORAGE_ARGS='-device ich9-ahci,id=ahci -drive if=none,id=sata,file=sata.img,format=raw -device ide-hd,drive=sata,bus=ahci.0'
```

## Clean

```sh
make clean
```
