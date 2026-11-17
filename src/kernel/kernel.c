#include <mydos.h>

// GRUB's Multiboot v1 handoff uses these flag bits to mark optional fields. 
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u
#define MULTIBOOT_INFO_CMDLINE 0x00000004u
#define MULTIBOOT_INFO_MODULES 0x00000008u

// Multiboot v1 module descriptors contain [start, end), name pointer, reserved.
typedef struct __attribute__((packed)) {
    uint32_t start;
    uint32_t end;
    uint32_t command_line;
    uint32_t reserved;
} multiboot_module_t;

static int multiboot_installer_requested(unsigned int magic, unsigned int mbi) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC || mbi == 0) {
        return 0;
    }

    // The command-line pointer is word 4 in the v1 boot-info structure.
    const uint32_t *info = (const uint32_t *)(uintptr_t)mbi;
    if ((info[0] & MULTIBOOT_INFO_CMDLINE) == 0 || info[4] == 0) {
        return 0;
    }

    return strcmp((const char *)(uintptr_t)info[4], "installer") == 0;
}

static int multiboot_find_installer_payload(unsigned int mbi,
                                            const uint8_t **payload,
                                            uint32_t *payload_size) {
    // Module count and descriptor-list address are words 5 and 6.
    const uint32_t *info = (const uint32_t *)(uintptr_t)mbi;
    if ((info[0] & MULTIBOOT_INFO_MODULES) == 0 || info[5] == 0 || info[6] == 0) {
        return 0;
    }

    uint32_t module_count = info[5];
    uint32_t modules_address = info[6];
    // Refuse a count whose descriptor array would wrap the 32-bit address space.
    if (module_count > (0xFFFFFFFFu - modules_address) / sizeof(multiboot_module_t)) {
        return 0;
    }

    const multiboot_module_t *modules =
        (const multiboot_module_t *)(uintptr_t)modules_address;
    for (uint32_t i = 0; i < module_count; i++) {
        const multiboot_module_t *module = &modules[i];
        if (module->command_line == 0 || module->end <= module->start) {
            continue;
        }

        const char *name = (const char *)(uintptr_t)module->command_line;
        // GRUB puts the text after `module <file>` in this module's name field.
        if (strcmp(name, "mydos-install") == 0) {
            *payload = (const uint8_t *)(uintptr_t)module->start;
            *payload_size = module->end - module->start;
            return 1;
        }
    }

    return 0;
}

static const char *format_error_message(FRESULT result) {
    switch (result) {
        case FR_DISK_ERR:
            return "FAT32 format failed: disk I/O error. :(";
        case FR_NOT_READY:
            return "FAT32 format failed: disk not ready. :(";
        case FR_WRITE_PROTECTED:
            return "FAT32 format failed: disk is write-protected. :(";
        case FR_MKFS_ABORTED:
            return "FAT32 format failed: disk could not be formatted. :(";
        default:
            return "FAT32 format failed: filesystem error. :(";
    }
}

void KernelEntryPoint(unsigned int magic, unsigned int mbi) {
    gdt_init();
    idt_init();
    fpu_init();
    keyboard_load_layout("de");

    vga_init_multiboot(mbi);
    clear_screen();

    // Keep installer startup separate so it never triggers the shell's auto-format prompt.
    if (multiboot_installer_requested(magic, mbi)) {
        const uint8_t *payload = 0;
        uint32_t payload_size = 0;
        if (!multiboot_find_installer_payload(mbi, &payload, &payload_size)) {
            installer_main(0, 0);
        } else {
            installer_main(payload, payload_size);
        }
        return;
    }

    print_logo();

    int cursor_y = 13;

    FRESULT fs_result = fs_init();

    if (fs_result == FR_NO_FILESYSTEM) {
        print("No recognized filesystem. Format this disk to FAT32? [y/N]", YELLOW, 0, cursor_y);
        scrl(&cursor_y);

        char answer;
        do {
            answer = keyboard_getchar();
        } while (answer != 'y' && answer != 'Y' && answer != 'n' && answer != 'N');

        if (answer == 'y' || answer == 'Y') {
            fs_result = fs_format();
            if (fs_result != FR_OK) {
                print(format_error_message(fs_result), RED, 0, cursor_y);
                scrl(&cursor_y);
            }
        } else {
            print("Disk left unchanged. :)", WHITE, 0, cursor_y);
            scrl(&cursor_y);
        }
    } else if (fs_result == FR_NOT_READY) {
        print("No usable disk detected. :(", RED, 0, cursor_y);
        scrl(&cursor_y);
    } else if (fs_result != FR_OK) {
        print("Unfortunately, MyDOS could not access the disk. :(", RED, 0, cursor_y);
        scrl(&cursor_y);
    }

    // If the filesystem is ready, start the shell.
    if (fs_result == FR_OK) {shell_run(mbi, cursor_y);}
    // Warn the user if the filesystem is not ready, but still start the shell.
    else {
        print("Warning: The filesystem is not ready. Some commands may fail.", YELLOW, 0, cursor_y);
        scrl(&cursor_y);
        shell_run(mbi, cursor_y);
    }
    

}

void scrl(int *cursor_y) {
    (*cursor_y)++;
    scroll_screen(cursor_y);
}
