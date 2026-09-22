MyDOS is a simple 32-bit x86 hobby operating system written in C and Assembly. It boots with GRUB and runs in QEMU.

Current version: v0.2.1 beta
Current shell version: trml v0.1.2 beta

Available commands:
help [command] - Show available commands or help
clear [-dl|--draw-logo] - Clear screen and optional logo
echo [-c <color>|--color <color>] <text> - Print colored text
change-theme <default|green|ocean|sunset> - Change shell theme
date [-t|--time] [-d|--date] [-12|--12h] - Show time or date
uptime [-p|--pretty] [-s|--seconds] - Show system uptime
sysinfo - Show kernel and terminal version, CPU, and RAM
shlogo - Display MyDOS logo
rbt - Reboot computer
shtdwn - Stops processes and halts the CPU

Supported echo colors: blue, green, cyan, red, magenta, brown, light_grey, dark_grey, light_blue, light_green, light_cyan, light_red, light_magenta, yellow, white.

Project Structure

Prerequisites: gcc, nasm, ld, grub-mkrescue, xorriso, qemu-system-i386

Build ISO:
make

Run in QEMU:
make run

Clean:
make clean

This is still a beta hobby OS. It is small and experimental but it works as a basic OS shell and demo.

Note from me: this project is getting really hard to work on, so I'd really appreciate it if anyone contributed to this project :)
