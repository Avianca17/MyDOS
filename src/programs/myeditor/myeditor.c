#include "myeditor.h"
#include "kernel.h"
#include "../../kernel/drivers/keyboard.h"
#include "../../fs/fatfs/source/ff.h"
#include "../../kernel/drivers/vga.h"

#define EDITOR_ROWS 21
#define EDITOR_COLUMNS 78
#define EDITOR_START_X 1
#define EDITOR_START_Y 3
#define EDITOR_FILENAME_MAX 32
#define EDITOR_STATUS_X 36
#define EDITOR_STATUS_Y 1
#define EDITOR_STATUS_WIDTH 43

static char document[EDITOR_ROWS][EDITOR_COLUMNS];
static int line_lengths[EDITOR_ROWS];
static char load_buffer[EDITOR_ROWS][EDITOR_COLUMNS];
static int load_line_lengths[EDITOR_ROWS];
static int line_count;
static char current_filename[EDITOR_FILENAME_MAX + 1];
static int has_filename;
static int has_unsaved_changes;

static void show_status(const char *message, unsigned char color) {
    char blank[EDITOR_STATUS_WIDTH + 1];
    for (int i = 0; i < EDITOR_STATUS_WIDTH; i++) {
        blank[i] = ' ';
    }
    blank[EDITOR_STATUS_WIDTH] = '\0';
    print(blank, LIGHT_GREY, EDITOR_STATUS_X, EDITOR_STATUS_Y);
    print(message, color, EDITOR_STATUS_X, EDITOR_STATUS_Y);
}

static const char *save_error_message(FRESULT result) {
    switch (result) {
        case FR_DISK_ERR: return "Save failed: disk error.";
        case FR_NOT_READY: return "Save failed: disk not ready.";
        case FR_NO_PATH: return "Save failed: path not found.";
        case FR_INVALID_NAME: return "Save failed: invalid filename.";
        case FR_DENIED: return "Save failed: write denied.";
        case FR_WRITE_PROTECTED: return "Save failed: disk is write-protected.";
        case FR_NO_FILESYSTEM: return "Save failed: no filesystem.";
        default: return "Save failed: filesystem error.";
    }
}

static int read_filename(char *filename, const char *prompt) {
    int length = 0;
    int prompt_length = 0;
    filename[0] = '\0';

    while (prompt[prompt_length] != '\0') {
        prompt_length++;
    }
    print(prompt, WHITE, EDITOR_STATUS_X, EDITOR_STATUS_Y);
    move_cursor(EDITOR_STATUS_X + prompt_length, EDITOR_STATUS_Y);

    while (1) {
        char key = keyboard_getchar();
        if (key == 0) continue;

        if (key == 0x1B) {
            return 0;
        }
        if (key == '\n' || key == '\r') {
            filename[length] = '\0';
            return length > 0;
        }
        if (key == '\b') {
            if (length > 0) {
                length--;
                filename[length] = '\0';
                print(" ", WHITE,
                      EDITOR_STATUS_X + prompt_length + length, EDITOR_STATUS_Y);
                move_cursor(EDITOR_STATUS_X + prompt_length + length,
                            EDITOR_STATUS_Y);
            }
        } else if (key >= 32 && key <= 126 && length < EDITOR_FILENAME_MAX) {
            filename[length++] = key;
            filename[length] = '\0';
            print((char[]){key, '\0'}, WHITE,
                  EDITOR_STATUS_X + prompt_length + length - 1, EDITOR_STATUS_Y);
            move_cursor(EDITOR_STATUS_X + prompt_length + length,
                        EDITOR_STATUS_Y);
        }
    }
}

static int confirm_overwrite(void) {
    show_status("File exists. Overwrite? (y/n)", YELLOW);
    while (1) {
        char key = keyboard_getchar();
        if (key == 0) continue;
        if (key == 'y' || key == 'Y') return 1;
        if (key == 'n' || key == 'N' || key == 0x1B) return 0;
    }
}

static int confirm_discard_changes(void) {
    show_status("Discard unsaved changes? (y/n)", YELLOW);
    while (1) {
        char key = keyboard_getchar();
        if (key == 0) continue;
        if (key == 'y' || key == 'Y') return 1;
        if (key == 'n' || key == 'N' || key == 0x1B) return 0;
    }
}

static FRESULT save_document(const char *filename) {
    static const char newline[] = "\r\n";
    FIL file;
    FRESULT result = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (result != FR_OK) return result;

    for (int row = 0; row < line_count; row++) {
        if (line_lengths[row] > 0) {
            UINT bytes_written = 0;
            result = f_write(&file, document[row],
                             (UINT)line_lengths[row], &bytes_written);
            if (result != FR_OK) break;
            if (bytes_written != (UINT)line_lengths[row]) {
                result = FR_DISK_ERR;
                break;
            }
        }

        if (row + 1 < line_count) {
            UINT bytes_written = 0;
            result = f_write(&file, newline, sizeof(newline) - 1,
                             &bytes_written);
            if (result != FR_OK) break;
            if (bytes_written != sizeof(newline) - 1) {
                result = FR_DISK_ERR;
                break;
            }
        }
    }

    FRESULT close_result = f_close(&file);
    return result == FR_OK ? close_result : result;
}

static void save_file(void) {
    char filename[EDITOR_FILENAME_MAX + 1];
    if (has_filename) {
        int i = 0;
        while (current_filename[i] != '\0') {
            filename[i] = current_filename[i];
            i++;
        }
        filename[i] = '\0';
    } else {
        show_status("", LIGHT_GREY);
        if (!read_filename(filename, "Save as: ")) {
            show_status("Save cancelled.", YELLOW);
            return;
        }
    }

    FILINFO info;
    FRESULT stat_result = f_stat(filename, &info);
    if (stat_result == FR_OK) {
        if (info.fattrib & AM_DIR) {
            show_status("Cannot save: path is a directory.", RED);
            return;
        }
        if (!has_filename && !confirm_overwrite()) {
            show_status("Save cancelled.", YELLOW);
            return;
        }
    } else if (stat_result != FR_NO_FILE) {
        show_status(save_error_message(stat_result), RED);
        return;
    }

    FRESULT result = save_document(filename);
    if (result != FR_OK) {
        show_status(save_error_message(result), RED);
        return;
    }

    int i = 0;
    while (filename[i] != '\0') {
        current_filename[i] = filename[i];
        i++;
    }
    current_filename[i] = '\0';
    has_filename = 1;
    has_unsaved_changes = 0;

    char message[EDITOR_FILENAME_MAX + 8];
    const char prefix[] = "Saved: ";
    int position = 0;
    while (prefix[position] != '\0') {
        message[position] = prefix[position];
        position++;
    }
    for (int j = 0; filename[j] != '\0'; j++) {
        message[position++] = filename[j];
    }
    message[position] = '\0';
    show_status(message, GREEN);
}

typedef enum {
    EDITOR_LOAD_OK,
    EDITOR_LOAD_TOO_LARGE,
    EDITOR_LOAD_INVALID_TEXT,
    EDITOR_LOAD_FILESYSTEM_ERROR
} editor_load_status;

static editor_load_status load_document(const char *filename,
                                        FRESULT *filesystem_error) {
    for (int y = 0; y < EDITOR_ROWS; y++) {
        load_line_lengths[y] = 0;
        for (int x = 0; x < EDITOR_COLUMNS; x++) {
            load_buffer[y][x] = '\0';
        }
    }

    int row = 0;
    int column = 0;
    int loaded_line_count = 1;
    int pending_newline = 0;
    editor_load_status status = EDITOR_LOAD_OK;
    FIL file;

    *filesystem_error = f_open(&file, filename, FA_READ);
    if (*filesystem_error != FR_OK) {
        return EDITOR_LOAD_FILESYSTEM_ERROR;
    }

    char buffer[128];
    UINT bytes_read;
    for (;;) {
        FRESULT result = f_read(&file, buffer, sizeof(buffer), &bytes_read);
        if (result != FR_OK) {
            *filesystem_error = result;
            status = EDITOR_LOAD_FILESYSTEM_ERROR;
            break;
        }
        if (bytes_read == 0) {
            break;
        }

        for (UINT i = 0; i < bytes_read; i++) {
            unsigned char character = (unsigned char)buffer[i];
            if (character == '\r') {
                continue;
            }
            if (character == '\n') {
                if (pending_newline) {
                    if (row + 1 >= EDITOR_ROWS) {
                        status = EDITOR_LOAD_TOO_LARGE;
                        break;
                    }
                    row++;
                    column = 0;
                    loaded_line_count = row + 1;
                }
                pending_newline = 1;
                continue;
            }

            if (character < 32 || character > 126) {
                status = EDITOR_LOAD_INVALID_TEXT;
                break;
            }
            if (pending_newline) {
                if (row + 1 >= EDITOR_ROWS) {
                    status = EDITOR_LOAD_TOO_LARGE;
                    break;
                }
                row++;
                column = 0;
                loaded_line_count = row + 1;
                pending_newline = 0;
            }
            if (column >= EDITOR_COLUMNS) {
                status = EDITOR_LOAD_TOO_LARGE;
                break;
            }
            load_buffer[row][column++] = (char)character;
            load_line_lengths[row] = column;
        }
        if (status != EDITOR_LOAD_OK) {
            break;
        }
    }

    FRESULT close_result = f_close(&file);
    if (status == EDITOR_LOAD_OK && close_result != FR_OK) {
        *filesystem_error = close_result;
        status = EDITOR_LOAD_FILESYSTEM_ERROR;
    }
    if (status != EDITOR_LOAD_OK) {
        return status;
    }

    for (int y = 0; y < EDITOR_ROWS; y++) {
        line_lengths[y] = load_line_lengths[y];
        for (int x = 0; x < EDITOR_COLUMNS; x++) {
            document[y][x] = load_buffer[y][x];
        }
    }
    line_count = loaded_line_count;
    int i = 0;
    while (filename[i] != '\0') {
        current_filename[i] = filename[i];
        i++;
    }
    current_filename[i] = '\0';
    has_filename = 1;
    has_unsaved_changes = 0;
    return EDITOR_LOAD_OK;
}

static const char *open_error_message(FRESULT result) {
    switch (result) {
        case FR_DISK_ERR: return "Open failed: disk error.";
        case FR_NOT_READY: return "Open failed: disk not ready.";
        case FR_NO_PATH: return "Open failed: path not found.";
        case FR_INVALID_NAME: return "Open failed: invalid filename.";
        case FR_NO_FILE: return "Open failed: file not found.";
        case FR_DENIED: return "Open failed: cannot read file.";
        case FR_NO_FILESYSTEM: return "Open failed: no filesystem.";
        default: return "Open failed: filesystem error.";
    }
}

static int open_file(void) {
    char filename[EDITOR_FILENAME_MAX + 1];
    if (has_unsaved_changes && !confirm_discard_changes()) {
        show_status("Open cancelled; changes kept.", YELLOW);
        return 0;
    }

    show_status("", LIGHT_GREY);
    if (!read_filename(filename, "Open: ")) {
        show_status("Open cancelled.", YELLOW);
        return 0;
    }

    FRESULT filesystem_error = FR_OK;
    editor_load_status status = load_document(filename, &filesystem_error);
    if (status == EDITOR_LOAD_TOO_LARGE) {
        show_status("Open failed: document too large.", RED);
        return 0;
    } else if (status == EDITOR_LOAD_INVALID_TEXT) {
        show_status("Open failed: unsupported text.", RED);
        return 0;
    } else if (status == EDITOR_LOAD_FILESYSTEM_ERROR) {
        show_status(open_error_message(filesystem_error), RED);
        return 0;
    }

    char message[EDITOR_FILENAME_MAX + 9];
    const char prefix[] = "Opened: ";
    int position = 0;
    while (prefix[position] != '\0') {
        message[position] = prefix[position];
        position++;
    }
    for (int i = 0; filename[i] != '\0'; i++) {
        message[position++] = filename[i];
    }
    message[position] = '\0';
    show_status(message, GREEN);
    return 1;
}

static void draw_document(void) {
    char display_line[EDITOR_COLUMNS + 1];

    for (int row = 0; row < EDITOR_ROWS; row++) {
        for (int column = 0; column < EDITOR_COLUMNS; column++) {
            display_line[column] = column < line_lengths[row]
                ? document[row][column]
                : ' ';
        }
        display_line[EDITOR_COLUMNS] = '\0';
        print(display_line, WHITE, EDITOR_START_X, EDITOR_START_Y + row);
    }
}

static void insert_character(int row, int column, char character) {
    if (line_lengths[row] >= EDITOR_COLUMNS) return;

    for (int x = line_lengths[row]; x > column; x--) {
        document[row][x] = document[row][x - 1];
    }
    document[row][column] = character;
    line_lengths[row]++;
}

static void delete_character(int row, int column) {
    for (int x = column; x < line_lengths[row] - 1; x++) {
        document[row][x] = document[row][x + 1];
    }
    line_lengths[row]--;
}

static int split_line(int row, int column) {
    if (line_count >= EDITOR_ROWS) return 0;

    for (int y = line_count; y > row + 1; y--) {
        line_lengths[y] = line_lengths[y - 1];
        for (int x = 0; x < line_lengths[y]; x++) {
            document[y][x] = document[y - 1][x];
        }
    }

    int next_length = line_lengths[row] - column;
    for (int x = 0; x < next_length; x++) {
        document[row + 1][x] = document[row][column + x];
    }
    line_lengths[row + 1] = next_length;
    line_lengths[row] = column;
    line_count++;
    return 1;
}

static int join_with_previous_line(int row) {
    int previous_length = line_lengths[row - 1];
    if (previous_length + line_lengths[row] > EDITOR_COLUMNS) return 0;

    for (int x = 0; x < line_lengths[row]; x++) {
        document[row - 1][previous_length + x] = document[row][x];
    }
    line_lengths[row - 1] += line_lengths[row];

    for (int y = row; y < line_count - 1; y++) {
        line_lengths[y] = line_lengths[y + 1];
        for (int x = 0; x < line_lengths[y]; x++) {
            document[y][x] = document[y + 1][x];
        }
    }
    line_count--;
    line_lengths[line_count] = 0;
    return 1;
}

void ui(void) {
    clear_screen();
    for (int i = 0; i <= 24; i++) {
        int x_temp = 0;
        int x_temp2 = 79;
        print("|", LIGHT_GREY, x_temp, i);
        ++x_temp;
        print("|", LIGHT_GREY, x_temp2, i);
        ++x_temp2;
    }
    for (int i = 0; i <= 79; i++) {
        int y_temp = 0;
        int y_temp2 = 24;
        print("=", LIGHT_GREY, i, y_temp);
        ++y_temp;
        print("=", LIGHT_GREY, i, y_temp2);
        ++y_temp2;
    }
    print("MYEDITOR", GREEN, 2, 0);
    print("F1", LIGHT_GREY, 2, 1); print("Save", LIGHT_GREY, 5, 1);
    print("F2", LIGHT_GREY, 15, 1); print("Open", LIGHT_GREY, 18, 1);
    print("F3", LIGHT_GREY, 28, 1); print("Exit", LIGHT_GREY, 31, 1);
    int y_t_t = 0;
    while (y_t_t <= 79) {
        print("-", LIGHT_GREY, y_t_t, 2);
        ++y_t_t;
    }
    move_cursor(1, 3);
}

void myeditor(void) {
    for (int row = 0; row < EDITOR_ROWS; row++) {
        line_lengths[row] = 0;
    }
    line_count = 1;
    current_filename[0] = '\0';
    has_filename = 0;
    has_unsaved_changes = 0;

    ui();
    int cursor_x = 0;
    int cursor_y = 0;
    draw_document();

    while (1) {
        char key = keyboard_getchar();

        if (key == KEY_F3) break;
        if (key == 0) continue;

        if (key == KEY_F1) {
            save_file();
        } else if (key == KEY_F2) {
            if (open_file()) {
                cursor_x = 0;
                cursor_y = 0;
            }
        } else if (key == KEY_ARROW_UP) {
            if (cursor_y > 0) {
                cursor_y--;
                if (cursor_x > line_lengths[cursor_y]) {
                    cursor_x = line_lengths[cursor_y];
                }
            }
        } else if (key == KEY_ARROW_DOWN) {
            if (cursor_y + 1 < line_count) {
                cursor_y++;
                if (cursor_x > line_lengths[cursor_y]) {
                    cursor_x = line_lengths[cursor_y];
                }
            }
        } else if (key == KEY_ARROW_LEFT) {
            if (cursor_x > 0) {
                cursor_x--;
            } else if (cursor_y > 0) {
                cursor_y--;
                cursor_x = line_lengths[cursor_y];
            }
        } else if (key == KEY_ARROW_RIGHT) {
            if (cursor_x < line_lengths[cursor_y]) {
                cursor_x++;
            } else if (cursor_y + 1 < line_count) {
                cursor_y++;
                cursor_x = 0;
            }
        } else if (key == '\b') {
            if (cursor_x > 0) {
                cursor_x--;
                delete_character(cursor_y, cursor_x);
                has_unsaved_changes = 1;
            } else if (cursor_y > 0 &&
                       join_with_previous_line(cursor_y)) {
                cursor_y--;
                cursor_x = line_lengths[cursor_y];
                has_unsaved_changes = 1;
            }
        } else if (key == '\n' || key == '\r') {
            if (split_line(cursor_y, cursor_x)) {
                cursor_y++;
                cursor_x = 0;
                has_unsaved_changes = 1;
            }
        } else if (key >= 32 && key <= 126) {
            if (line_lengths[cursor_y] < EDITOR_COLUMNS) {
                insert_character(cursor_y, cursor_x, key);
                cursor_x++;
                has_unsaved_changes = 1;
            }
        }

        draw_document();
        int display_x = EDITOR_START_X + cursor_x;
        if (display_x > EDITOR_START_X + EDITOR_COLUMNS - 1) {
            display_x = EDITOR_START_X + EDITOR_COLUMNS - 1;
        }
        move_cursor(display_x, EDITOR_START_Y + cursor_y);
    }

    clear_screen();
}