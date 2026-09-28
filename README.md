MyDOS is a simple 32-bit x86 hobby operating system written in C and Assembly. It boots with GRUB, supports FAT32 using FatFs, and runs in QEMU.

Current version: v0.3
Current shell version: trml v0.2

Available commands:

Shell:
- help [command] - Show available commands or help for a specific command
- clear [-dl|--draw-logo] - Clear screen and optional logo
- echo [-c <color>|--color <color>] <text> - Print colored text
- change-theme <default|matrix|ocean|sunset> - Change shell theme
- date [-t|--time] [-d|--date] [-12|--12h] - Show time or date
- uptime [-p|--pretty] [-s|--seconds] - Show system uptime
- sysinfo - Show kernel and shell version, CPU vendor, and RAM
- shlogo - Display MyDOS logo

Filesystem (FAT32):
- ls - List files and folders in current directory
- lsdrv - List available storage drives
- mkfile <filename> - Create a new file
- mkdir <dirname> - Create a new directory
- rm [-r] <path> - Delete a file (or directory with -r)

System:
- rbt - Reboot computer
- shtdwn - Stop processes and halt the CPU

Supported echo colors:
blue, green, cyan, red, magenta, brown, light_grey, dark_grey, light_blue, light_green, light_cyan, light_red, light_magenta, yellow, white.

Project Structure:
```MyDOS/
├── iso/
│   └── boot/
│       └── grub/
│           └── grub.cfg
├── linker.ld
├── Makefile
├── README.md
└── src/
    ├── boot.asm
    ├── fs/
    │   └── fatfs/             <-- FatFs library and disk drivers
    ├── kernel/
    │   ├── drivers/
    │   │   ├── keyboard.c/h
    │   │   ├── rtc.c/h
    │   │   └── vga.c/h
    │   ├── helper/
    │   │   ├── fshelpers/     <-- FAT32 helper functions
    │   │   │   ├── fshelpers.c
    │   │   │   └── fshelpers.h
    │   │   ├── helpers.c
    │   │   └── helpers.h
    │   ├── kernel.c
    │   └── kernel.h
    └── lib/
        ├── hardware.c/h
        ├── shtdwn.c/h
        └── string.c/h``

Prerequisites:
gcc, nasm, ld, grub-mkrescue, xorriso, qemu-system-i386

Build ISO:
make

Run in QEMU:
make run

Clean:
make clean

This is still a beta hobby OS. It is small and experimental but it works as a basic OS shell and demo.

Note from me: this project is getting really hard to work on, so I'd really appreciate it if anyone contributed to this project :)
