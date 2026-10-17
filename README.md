MyDOS is a simple 32-bit x86 hobby operating system written in C and Assembly. It boots with GRUB, supports FAT32 using FatFs, and runs in QEMU.

MyDOS is a freestanding, bare-metal kernel. Its storage layer supports four
legacy ATA/IDE disks and up to four disks on a PCI AHCI SATA controller.
Legacy ATA includes CHS-only and LBA28 disks. SATA is supported through native
AHCI controllers.

Current version: v0.3
Current shell version: trml v0.2

Available commands:

Shell:
- help [command] - Show available commands or help for a specific command
- beep - Play a short tone through the legacy PC speaker
- clear [-dl|--draw-logo] - Clear screen and optional logo
- echo [-c <color>|--color <color>] <text> - Print colored text
- change-theme <default|matrix|ocean|sunset> - Change shell theme
- loadkeys <us|de|fr> - Load a built-in keyboard layout; `loadkeys status` reports the active layout (German QWERTZ is the default)
- date [-t|--time] [-d|--date] [-12|--12h] - Show time or date
- uptime [-p|--pretty] [-s|--seconds] - Show system uptime
- sysinfo - Show kernel and shell version, CPU vendor, and RAM
- fetch - Show system information and uptime beside the MyDOS logo
- shlogo - Display MyDOS logo

Filesystem (FAT32):
- ls - List files and folders in current directory
- lsdrv - List available storage drives
- mkfile <filename> - Create a new file
- mkdir <dirname> - Create a new directory
- rm [-r] <path> - Delete a file (or directory with -r)
- myeditor - Edit text; press F1 to save (first save prompts for a filename)

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
        └── string.c/h```

Prerequisites:
gcc, nasm, ld, grub-mkrescue, xorriso, qemu-system-i386

Build ISO:
make

The build installs only the GRUB modules needed for the Multiboot kernel,
omits GRUB fonts, locales, and themes, and creates a BIOS-only ISO. If GRUB is
installed outside `/usr/lib/grub/i386-pc`, set `GRUB_BIOS_DIR` when building.

Run in QEMU:
make run

QEMU's PC speaker audio is routed to the host using PulseAudio by default. If
your QEMU installation uses another audio backend, override `QEMU_AUDIO_ARGS`
with the appropriate `-audiodev` backend and `pcspk-audiodev` machine option.
For example, use `-audiodev pipewire,id=pcspk -machine pc,pcspk-audiodev=pcspk`
when using PipeWire.

To test native SATA/AHCI storage, create a blank disk image:

```sh
qemu-img create -f raw sata.img 128M
```

Then run QEMU with an AHCI controller attached:

```sh
make run QEMU_STORAGE_ARGS='-device ich9-ahci,id=ahci -drive if=none,id=sata,file=sata.img,format=raw -device ide-hd,drive=sata,bus=ahci.0'
```

Use `lsdrv` to list detected disks. FAT32 volumes mounted at startup become the
shell's current volume; blank disks are left unchanged unless formatting is
confirmed at the startup prompt.

Clean:
make clean

This is still a beta hobby OS. It is small and experimental but it works as a basic OS shell and demo.

The `beep` command uses the legacy PC speaker interface. A tone is audible only
on machines or emulators with compatible PC-speaker sound output.

Note from me: this project is getting really hard to work on, so I'd really appreciate it if anyone contributed to this project :)
