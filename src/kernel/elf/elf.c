#include <mydos.h>
#include <elf/elf.h>
#include <lib/user_api.h>
#include <ff.h>

#define ELF_PT_LOAD 1u
#define ELF_MAX_PHDR 16u
#define ELF_USER_BASE 0x02000000u
#define ELF_MIN_VADDR 0x00001000u
#define ELF_MAX_SEGMENT 0x01000000u

extern uint8_t _kernel_end;

static const mydos_user_api_t user_api = {
    1,
    print,
    scrl,
    keyboard_getchar
};

static uint16_t elf_read16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint32_t elf_read32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static int elf_read_exact(FIL *file, void *buffer, uint32_t length) {
    UINT read = 0;
    return f_read(file, buffer, length, &read) == FR_OK && read == length;
}

static int elf_range_in_mmap(unsigned int mbi, uint32_t start, uint32_t end) {
    unsigned int *info = (unsigned int *)mbi;
    if ((info[0] & 0x40u) == 0) {
        return 0;
    }

    uint32_t offset = 0;
    uint32_t length = info[11];
    uint32_t address = info[12];
    while (offset + 20 <= length) {
        struct mmap_entry {
            uint32_t size;
            uint32_t address_low;
            uint32_t address_high;
            uint32_t length_low;
            uint32_t length_high;
            uint32_t type;
        } __attribute__((packed)) *entry =
            (struct mmap_entry *)(address + offset);

        if (entry->type == 1u && entry->address_high == 0 &&
            entry->address_low <= start &&
            end <= entry->address_low + entry->length_low) {
            return 1;
        }
        offset += entry->size + 4u;
    }
    return 0;
}

static int elf_range_usable(unsigned int mbi, uint32_t start, uint32_t end) {
    if (mbi == 0 || start >= end) {
        return 0;
    }
    unsigned int *info = (unsigned int *)mbi;
    uint32_t ram_end = 0;
    if ((info[0] & 0x01u) != 0) {
        ram_end = 0x00100000u + (info[1] + info[2]) * 1024u;
    }
    if (ram_end != 0 && end > ram_end - 0x00100000u) {
        return 0;
    }
    if ((info[0] & 0x40u) != 0 && !elf_range_in_mmap(mbi, start, end)) {
        return 0;
    }
    return 1;
}

int elf_run(const char *path, unsigned int mbi, int argc, char **argv,
            int *exit_code) {
    if (path == 0 || exit_code == 0) {
        return 0;
    }
    *exit_code = 0;

    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK) {
        return 0;
    }

    uint8_t header[52];
    if (!elf_read_exact(&file, header, sizeof(header))) {
        f_close(&file);
        return 0;
    }
    if (header[0] != 0x7F || header[1] != 'E' || header[2] != 'L' ||
        header[3] != 'F' || header[4] != 1 || header[5] != 1 ||
        elf_read16(header + 16) != 2 || elf_read16(header + 18) != 3 ||
        elf_read32(header + 20) != 1) {
        f_close(&file);
        return 0;
    }

    uint32_t entry_offset = elf_read32(header + 24);
    uint32_t phdr_offset = elf_read32(header + 28);
    uint16_t phdr_size = elf_read16(header + 42);
    uint16_t phdr_count = elf_read16(header + 44);
    if (phdr_size < 32 || phdr_count == 0 || phdr_count > ELF_MAX_PHDR ||
        phdr_offset == 0) {
        f_close(&file);
        return 0;
    }

    uint32_t maximum = 0;
    uint8_t phdr[32];
    for (uint16_t i = 0; i < phdr_count; i++) {
        if (f_lseek(&file, phdr_offset + i * phdr_size) != FR_OK ||
            !elf_read_exact(&file, phdr, sizeof(phdr))) {
            f_close(&file);
            return 0;
        }
        if (elf_read32(phdr) != ELF_PT_LOAD) {
            continue;
        }
        uint32_t vaddr = elf_read32(phdr + 8);
        uint32_t file_size = elf_read32(phdr + 16);
        uint32_t memory_size = elf_read32(phdr + 20);
        if (vaddr < ELF_MIN_VADDR || file_size > memory_size ||
            memory_size == 0 || memory_size > ELF_MAX_SEGMENT ||
            vaddr + memory_size < vaddr) {
            f_close(&file);
            return 0;
        }
        if (vaddr + memory_size > maximum) {
            maximum = vaddr + memory_size;
        }
    }
    if (maximum == 0) {
        f_close(&file);
        return 0;
    }

    uint32_t kernel_end = (uint32_t)&_kernel_end;
    uint32_t load_base = ELF_USER_BASE;
    if (load_base < kernel_end + 0x00100000u) {
        load_base = (kernel_end + 0x00100000u) & 0xFFF00000u;
    }
    if (load_base < kernel_end + 0x00100000u ||
        !elf_range_usable(mbi, load_base, load_base + maximum) ||
        entry_offset < ELF_MIN_VADDR || entry_offset >= maximum) {
        f_close(&file);
        return 0;
    }

    for (uint16_t i = 0; i < phdr_count; i++) {
        if (f_lseek(&file, phdr_offset + i * phdr_size) != FR_OK ||
            !elf_read_exact(&file, phdr, sizeof(phdr)) ||
            elf_read32(phdr) != ELF_PT_LOAD) {
            continue;
        }
        uint32_t file_offset = elf_read32(phdr + 4);
        uint32_t vaddr = elf_read32(phdr + 8);
        uint32_t file_size = elf_read32(phdr + 16);
        uint32_t memory_size = elf_read32(phdr + 20);
        uint8_t *destination = (uint8_t *)(load_base + vaddr);

        if (file_size != 0) {
            if (f_lseek(&file, file_offset) != FR_OK ||
                !elf_read_exact(&file, destination, file_size)) {
                f_close(&file);
                return 0;
            }
        }
        if (memory_size > file_size) {
            memset(destination + file_size, 0, memory_size - file_size);
        }
    }
    f_close(&file);

    mydos_user_entry_t entry =
        (mydos_user_entry_t)(uintptr_t)(load_base + entry_offset);
    *exit_code = entry(argc, argv, &user_api);
    return 1;
}
