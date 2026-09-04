MyDOS is a simple 32-bit x86 hobby operating system and shell written in C and Assembly, booted via GRUB.

Current version: v0.2 beta
Current shell version: trml v0.1.1 beta


Here are the available commands as of the latest commit:

help [command] - Show available commands or command help
clear [-dl|--draw-logo] - Clear screen (optional: redraw logo)
echo [-c <color>|--color <color>] <text> - Print text in color
date [-t|--time] [-d|--date] [-12|--12h] - Display time or date
uptime [-p|--pretty] [-s|--seconds] - Show time since boot
sysinfo - Show kernel version, CPU, and RAM
shlogo - Display MyDOS logo
rbt - Reboot computer
shtdwn - Halt CPU for shutdown

Supported echo colors: blue, green, cyan, red, magenta, brown, light_grey, dark_grey, light_blue, light_green, light_cyan, light_red, light_magenta, yellow, white.

Prerequisites: gcc, nasm, ld, grub-mkrescue, xorriso, qemu-system-i386

Build ISO:
make

Run in QEMU:
make run

Clean:
make clean
