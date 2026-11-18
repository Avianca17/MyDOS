#include <cmd/cmd.h>

static void print_mbr_error(FRESULT result, int *cursor_y) {
    switch (result) {
        case FR_DISK_ERR:
            print("mk-mbr-part: Disk error while creating MBR!", RED, 0, *cursor_y);
            break;
        case FR_NOT_READY:
            print("mk-mbr-part: Disk is not ready!", RED, 0, *cursor_y);
            break;
        case FR_WRITE_PROTECTED:
            print("mk-mbr-part: Disk is write-protected!", RED, 0, *cursor_y);
            break;
        case FR_INVALID_PARAMETER:
            print("mk-mbr-part: Invalid partition parameter!", RED, 0, *cursor_y);
            break;
        case FR_NOT_ENOUGH_CORE:
            print("mk-mbr-part: Not enough memory!", RED, 0, *cursor_y);
            break;
        default:
            print("mk-mbr-part: MBR partitioning failed!", RED, 0, *cursor_y);
            break;
    }
    scrl(cursor_y);
}

static void print_format_error(FRESULT result, int *cursor_y) {
    switch (result) {
        case FR_DISK_ERR:
            print("mk-mbr-part: Disk error during format!", RED, 0, *cursor_y);
            break;
        case FR_NOT_READY:
            print("mk-mbr-part: Disk is not ready!", RED, 0, *cursor_y);
            break;
        case FR_WRITE_PROTECTED:
            print("mk-mbr-part: Disk is write-protected!", RED, 0, *cursor_y);
            break;
        case FR_INVALID_PARAMETER:
            print("mk-mbr-part: Invalid FAT32 format parameter!", RED, 0, *cursor_y);
            break;
        case FR_MKFS_ABORTED:
            print("mk-mbr-part: FAT32 format aborted!", RED, 0, *cursor_y);
            break;
        case FR_NOT_ENOUGH_CORE:
            print("mk-mbr-part: Not enough memory!", RED, 0, *cursor_y);
            break;
        default:
            print("mk-mbr-part: FAT32 format failed!", RED, 0, *cursor_y);
            break;
    }
    scrl(cursor_y);
}

void cmd_mk_mbr_part(char *args, int *cursor_y) {
    char *arguments = args;
    char *path = cmd_next_token(&arguments);
    BYTE volume;

    if (path == 0 ||
        cmd_next_token(&arguments) != 0 ||
        !cmd_parse_drive_root(path, &volume)) {
        print("Usage: mk-mbr-part <drive>:/", RED, 0, *cursor_y);
        scrl(cursor_y);
        return;
    }

    LBA_t plist[] = {100, 0};
    BYTE work[FF_MAX_SS];

    FRESULT result = f_fdisk(volume, plist, work);
    if (result != FR_OK) {
        print_mbr_error(result, cursor_y);
        return;
    }

    #if FF_MULTI_PARTITION
    VolToPart[volume].pd = volume;
    VolToPart[volume].pt = 1;
    #endif
    TCHAR volume_path[3] = {(TCHAR)('0' + volume), ':', '\0'};
    MKFS_PARM options = {
        .fmt = FM_FAT32,
        .n_fat = 1,
        .align = 0,
        .n_root = 0,
        .au_size = 0
    };
    result = f_mkfs(volume_path, &options, work, sizeof work);
    if (result == FR_OK) {
        print("mk-mbr-part: MBR and FAT32 setup complete.", GREEN, 0, *cursor_y);
        scrl(cursor_y);
    } else {
        print_format_error(result, cursor_y);
    }
}
