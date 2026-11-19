#include <mydos.h>
#include "font8x16.h"

#define MULTIBOOT_INFO_FRAMEBUFFER 0x00001000u
#define FRAMEBUFFER_CELL_WIDTH 8
#define FRAMEBUFFER_CELL_HEIGHT 16

typedef struct __attribute__((packed)) {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t symbols[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t framebuffer_color_info_padding;
    uint8_t red_position;
    uint8_t red_mask_size;
    uint8_t green_position;
    uint8_t green_mask_size;
    uint8_t blue_position;
    uint8_t blue_mask_size;
} multiboot_info_v1_t;

typedef struct {
    uintptr_t address;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint32_t x_offset;
    uint32_t y_offset;
    uint8_t bytes_per_pixel;
    uint8_t red_position;
    uint8_t red_mask_size;
    uint8_t green_position;
    uint8_t green_mask_size;
    uint8_t blue_position;
    uint8_t blue_mask_size;
} framebuffer_t;

static const uint8_t vga_palette[16][3] = {
    {0, 0, 0},       {0, 0, 170},       {0, 170, 0},       {0, 170, 170},
    {170, 0, 0},     {170, 0, 170},     {170, 85, 0},     {170, 170, 170},
    {85, 85, 85},    {85, 85, 255},     {85, 255, 85},    {85, 255, 255},
    {255, 85, 85},   {255, 85, 255},   {255, 255, 85},   {255, 255, 255}
};

static uint16_t screen_cells[VGA_MAX_WIDTH * VGA_MAX_HEIGHT];
static framebuffer_t framebuffer;
static uint32_t framebuffer_palette[16];
static uint32_t indexed_palette[256];
static int framebuffer_enabled;
static int active_screen_width = VGA_WIDTH;
static int active_screen_height = VGA_HEIGHT;
static int cursor_x = -1;
static int cursor_y = -1;

#define PRESENT_MAX_WIDTH 640
#define PRESENT_MAX_HEIGHT 400

static uint16_t present_x_map[PRESENT_MAX_WIDTH];
static uint16_t present_y_map[PRESENT_MAX_HEIGHT];
static uint32_t present_row_offsets[PRESENT_MAX_HEIGHT];
static uint32_t present_target_width;
static uint32_t present_target_height;
static uint32_t present_fb_width;
static uint32_t present_fb_height;
static uint32_t present_fb_pitch;
static uint8_t present_fb_bpp;
static int present_source_width;
static int present_source_height;
static int present_ready;

static unsigned char theme_text = WHITE;
static unsigned char theme_muted = LIGHT_GREY;
static unsigned char theme_error = RED;
static unsigned char theme_prompt = GREEN;
static unsigned char theme_title = BLUE;
static unsigned char theme_background = BLACK;

static unsigned char theme_color(unsigned char color) {
    switch (color) {
        case WHITE: return theme_text;
        case LIGHT_GREY: return theme_muted;
        case RED: return theme_error;
        case GREEN: return theme_prompt;
        case BLUE: return theme_title;
        case BLACK: return theme_background;
        default:
            return color;
        }
}

static unsigned char theme_attribute(unsigned char color) {
    return theme_color(color) | (theme_background << 4);
}

static uint32_t scale_channel(uint8_t value, uint8_t mask_size) {
    uint32_t max_value = (1u << mask_size) - 1u;
    return ((uint32_t)value * max_value + 127u) / 255u;
}

static uint32_t pack_color(uint8_t red, uint8_t green, uint8_t blue) {
    return (scale_channel(red, framebuffer.red_mask_size) << framebuffer.red_position) |
           (scale_channel(green, framebuffer.green_mask_size) << framebuffer.green_position) |
           (scale_channel(blue, framebuffer.blue_mask_size) << framebuffer.blue_position);
}

void vga_init_multiboot(unsigned int mbi) {
    framebuffer_enabled = 0;
    active_screen_width = VGA_WIDTH;
    active_screen_height = VGA_HEIGHT;
    cursor_x = -1;
    cursor_y = -1;
    if (mbi == 0) {
        return;
    }

    const multiboot_info_v1_t *info =
        (const multiboot_info_v1_t *)(uintptr_t)mbi;
    if ((info->flags & MULTIBOOT_INFO_FRAMEBUFFER) == 0 ||
        info->framebuffer_type != 1 ||
        (info->framebuffer_bpp != 24 && info->framebuffer_bpp != 32) ||
        info->framebuffer_addr == 0 ||
        (info->framebuffer_addr >> 32) != 0 ||
        info->framebuffer_width < VGA_WIDTH * FRAMEBUFFER_CELL_WIDTH ||
        info->framebuffer_height < VGA_HEIGHT * FRAMEBUFFER_CELL_HEIGHT ||
        info->red_mask_size == 0 || info->red_mask_size > 8 ||
        info->green_mask_size == 0 || info->green_mask_size > 8 ||
        info->blue_mask_size == 0 || info->blue_mask_size > 8 ||
        info->red_position + info->red_mask_size > info->framebuffer_bpp ||
        info->green_position + info->green_mask_size > info->framebuffer_bpp ||
        info->blue_position + info->blue_mask_size > info->framebuffer_bpp) {
        return;
    }

    uint8_t bytes_per_pixel = (info->framebuffer_bpp + 7u) / 8u;
    uint64_t minimum_pitch = (uint64_t)info->framebuffer_width * bytes_per_pixel;
    uint64_t framebuffer_end = info->framebuffer_addr +
        (uint64_t)info->framebuffer_pitch * info->framebuffer_height;
    if (minimum_pitch > info->framebuffer_pitch ||
        framebuffer_end > 0x100000000ull) {
        return;
    }

    framebuffer.address = (uintptr_t)info->framebuffer_addr;
    framebuffer.pitch = info->framebuffer_pitch;
    framebuffer.width = info->framebuffer_width;
    framebuffer.height = info->framebuffer_height;
    framebuffer.bytes_per_pixel = bytes_per_pixel;
    active_screen_width = framebuffer.width / FRAMEBUFFER_CELL_WIDTH;
    active_screen_height = framebuffer.height / FRAMEBUFFER_CELL_HEIGHT;
    if (active_screen_width > VGA_MAX_WIDTH) {
        active_screen_width = VGA_MAX_WIDTH;
    }
    if (active_screen_height > VGA_MAX_HEIGHT) {
        active_screen_height = VGA_MAX_HEIGHT;
    }
    framebuffer.x_offset = (framebuffer.width -
        active_screen_width * FRAMEBUFFER_CELL_WIDTH) / 2;
    framebuffer.y_offset = (framebuffer.height -
        active_screen_height * FRAMEBUFFER_CELL_HEIGHT) / 2;
    framebuffer.red_position = info->red_position;
    framebuffer.red_mask_size = info->red_mask_size;
    framebuffer.green_position = info->green_position;
    framebuffer.green_mask_size = info->green_mask_size;
    framebuffer.blue_position = info->blue_position;
    framebuffer.blue_mask_size = info->blue_mask_size;

    for (unsigned int color = 0; color < 16; color++) {
        framebuffer_palette[color] =
            pack_color(vga_palette[color][0], vga_palette[color][1],
                       vga_palette[color][2]);
    }
    framebuffer_enabled = 1;
}

int vga_screen_width(void) {
    return active_screen_width;
}

int vga_screen_height(void) {
    return active_screen_height;
}

int vga_framebuffer_available(void) {
    return framebuffer_enabled;
}

void vga_framebuffer_set_indexed_palette(const unsigned char *palette) {
    if (!framebuffer_enabled || palette == 0) {
        return;
    }

    for (unsigned int index = 0; index < 256; index++) {
        indexed_palette[index] =
            pack_color(palette[index * 3], palette[index * 3 + 1],
                       palette[index * 3 + 2]);
    }
}

static void present_build_cache(int width, int height) {
    uint32_t scale = 1;
    if (framebuffer.width >= PRESENT_MAX_WIDTH &&
        framebuffer.height >= PRESENT_MAX_HEIGHT) {
        scale = 2;
    }

    present_target_width = (uint32_t)width * scale;
    present_target_height = (uint32_t)height * scale;
    if (present_target_width > PRESENT_MAX_WIDTH) {
        present_target_width = PRESENT_MAX_WIDTH;
    }
    if (present_target_height > PRESENT_MAX_HEIGHT) {
        present_target_height = PRESENT_MAX_HEIGHT;
    }
    if (present_target_width > framebuffer.width) {
        present_target_width = framebuffer.width;
    }
    if (present_target_height > framebuffer.height) {
        present_target_height = framebuffer.height;
    }
    if (present_target_width == 0 || present_target_height == 0) {
        present_ready = 0;
        return;
    }

    uint32_t x_offset = (framebuffer.width - present_target_width) / 2;
    uint32_t y_offset = (framebuffer.height - present_target_height) / 2;

    for (uint32_t x = 0; x < present_target_width; x++) {
        present_x_map[x] = (uint16_t)(x * (uint32_t)width / present_target_width);
    }
    for (uint32_t y = 0; y < present_target_height; y++) {
        present_y_map[y] =
            (uint16_t)(y * (uint32_t)height / present_target_height);
        present_row_offsets[y] =
            (y_offset + y) * framebuffer.pitch + x_offset * framebuffer.bytes_per_pixel;
    }

    present_fb_width = framebuffer.width;
    present_fb_height = framebuffer.height;
    present_fb_pitch = framebuffer.pitch;
    present_fb_bpp = framebuffer.bytes_per_pixel;
    present_source_width = width;
    present_source_height = height;
    present_ready = 1;
}

void vga_framebuffer_present_indexed(const unsigned char *pixels, int width,
                                     int height) {
    if (!framebuffer_enabled || pixels == 0 || width <= 0 || height <= 0) {
        return;
    }

    if (!present_ready || present_fb_width != framebuffer.width ||
        present_fb_height != framebuffer.height ||
        present_fb_pitch != framebuffer.pitch ||
        present_fb_bpp != framebuffer.bytes_per_pixel ||
        present_source_width != width || present_source_height != height) {
        present_build_cache(width, height);
    }
    if (!present_ready) {
        return;
    }

    for (uint32_t y = 0; y < present_target_height; y++) {
        const unsigned char *source =
            pixels + (uintptr_t)present_y_map[y] * (uint32_t)width;
        uintptr_t row = framebuffer.address + present_row_offsets[y];

        if (framebuffer.bytes_per_pixel == 4) {
            uint32_t *destination = (uint32_t *)row;
            for (uint32_t x = 0; x < present_target_width; x++) {
                destination[x] = indexed_palette[source[present_x_map[x]]];
            }
        } else if (framebuffer.bytes_per_pixel == 2) {
            uint16_t *destination = (uint16_t *)row;
            for (uint32_t x = 0; x < present_target_width; x++) {
                destination[x] =
                    (uint16_t)indexed_palette[source[present_x_map[x]]];
            }
        } else {
            volatile uint8_t *destination = (volatile uint8_t *)row;
            for (uint32_t x = 0; x < present_target_width; x++) {
                uint32_t color = indexed_palette[source[present_x_map[x]]];
                volatile uint8_t *pixel =
                    destination + (uintptr_t)x * framebuffer.bytes_per_pixel;
                for (uint8_t byte = 0; byte < framebuffer.bytes_per_pixel; byte++) {
                    pixel[byte] = (uint8_t)(color >> (byte * 8));
                }
            }
        }
    }
}

static void framebuffer_pixel(int x, int y, uint32_t color) {
    uint32_t pixel_x = framebuffer.x_offset + (uint32_t)x;
    uint32_t pixel_y = framebuffer.y_offset + (uint32_t)y;
    uintptr_t offset = (uintptr_t)pixel_y * framebuffer.pitch +
                       (uintptr_t)pixel_x * framebuffer.bytes_per_pixel;
    volatile uint8_t *pixel =
        (volatile uint8_t *)(framebuffer.address + offset);
    for (uint8_t byte = 0; byte < framebuffer.bytes_per_pixel; byte++) {
        pixel[byte] = (uint8_t)(color >> (byte * 8));
    }
}

static void draw_framebuffer_cell(int index) {
    uint16_t cell = screen_cells[index];
    uint8_t character = (uint8_t)cell;
    uint8_t attribute = (uint8_t)(cell >> 8);

    int cell_x = (index % active_screen_width) * FRAMEBUFFER_CELL_WIDTH;
    int cell_y = (index / active_screen_width) * FRAMEBUFFER_CELL_HEIGHT;
    uint32_t foreground = framebuffer_palette[attribute & 0x0f];
    uint32_t background = framebuffer_palette[(attribute >> 4) & 0x0f];

    for (int glyph_y = 0; glyph_y < FRAMEBUFFER_CELL_HEIGHT; glyph_y++) {
        uint8_t glyph_row = font8x16[character][glyph_y];
        for (int glyph_x = 0; glyph_x < 8; glyph_x++) {
            uint32_t color =
                (glyph_row & (0x80u >> glyph_x)) ? foreground : background;
            framebuffer_pixel(cell_x + glyph_x, cell_y + glyph_y, color);
        }
    }
}

static void draw_framebuffer_cursor(void) {
    if (!framebuffer_enabled || cursor_x < 0 ||
        cursor_y < 0 || cursor_y >= active_screen_height) {
        return;
    }

    if (cursor_x >= active_screen_width) {
        return;
    }
    int index = cursor_y * active_screen_width + cursor_x;
    uint8_t attribute = (uint8_t)(screen_cells[index] >> 8);
    uint32_t color = framebuffer_palette[attribute & 0x0f];
    if ((attribute & 0x0f) == BLACK) {
        color = framebuffer_palette[WHITE];
    }

    int x = cursor_x * FRAMEBUFFER_CELL_WIDTH;
    int y = cursor_y * FRAMEBUFFER_CELL_HEIGHT;
    for (int row = FRAMEBUFFER_CELL_HEIGHT - 2;
         row < FRAMEBUFFER_CELL_HEIGHT; row++) {
        for (int column = 0; column < FRAMEBUFFER_CELL_WIDTH; column++) {
            framebuffer_pixel(x + column, y + row, color);
        }
    }
}

static void render_cell(int index) {
    if (framebuffer_enabled) {
        draw_framebuffer_cell(index);
    } else {
        volatile uint16_t *vga_buffer =
            (volatile uint16_t *)(uintptr_t)VGA_ADDRESS;
        vga_buffer[index] = screen_cells[index];
    }
}

static void write_cell(int index, uint16_t cell) {
    if (index < 0 || index >= active_screen_width * active_screen_height) {
        return;
    }

    screen_cells[index] = cell;
    render_cell(index);
    if (framebuffer_enabled &&
        index == cursor_y * active_screen_width + cursor_x) {
        draw_framebuffer_cursor();
    }
}

int change_theme(const char* name) {
    if (strcmp(name, "default") == 0) {
        theme_text = WHITE;
        theme_muted = LIGHT_GREY;
        theme_error = RED;
        theme_prompt = GREEN;
        theme_title = BLUE;
        theme_background = BLACK;
        return 1;
    }

    if (strcmp(name, "matrix") == 0) {
        theme_text = LIGHT_GREEN;
        theme_muted = GREEN;
        theme_error = GREEN;
        theme_prompt = LIGHT_GREEN;
        theme_title = WHITE;
        theme_background = BLACK;
        return 1;
    }

    if (strcmp(name, "ocean") == 0) {
        theme_text = WHITE;
        theme_muted = LIGHT_BLUE;
        theme_error = LIGHT_RED;
        theme_prompt = LIGHT_CYAN;
        theme_title = LIGHT_BLUE;
        theme_background = BLACK;
        return 1;
    }
    if (strcmp(name, "sunset") == 0) {
        theme_text = YELLOW;
        theme_muted = DARK_GREY;
        theme_error = LIGHT_RED;
        theme_prompt = LIGHT_MAGENTA;
        theme_title = RED;
        theme_background = BROWN;
        return 1;
    }

    return 0;
}

static void draw_logo_pattern(const char* const pattern[], int width, int height, unsigned char color, int x, int y) {
    color = theme_attribute(color);

    for (int row = 0; row < height; row++) {
        for (int column = 0; column < width; column++) {
            if (pattern[row][column] == ' ') continue;

            unsigned short character = (unsigned short)pattern[row][column];
            int screen_index = (y + row) * active_screen_width + x + column;

            write_cell(screen_index,
                       character | ((unsigned short)color << 8));
        }
    }
}

void move_cursor(int x, int y) {
    if (framebuffer_enabled) {
        if (cursor_x >= 0 && cursor_x < active_screen_width &&
            cursor_y >= 0 && cursor_y < active_screen_height) {
            render_cell(cursor_y * active_screen_width + cursor_x);
        }
        cursor_x = x;
        cursor_y = y;
        draw_framebuffer_cursor();
        return;
    }

    unsigned short position = y * active_screen_width + x;

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)0x0F),
          "Nd"((unsigned short)VGA_CTRL)
    );

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)(position & 0xFF)),
          "Nd"((unsigned short)VGA_DATA)
    );

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)0x0E),
          "Nd"((unsigned short)VGA_CTRL)
    );

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)((position >> 8) & 0xFF)),
          "Nd"((unsigned short)VGA_DATA)
    );
}

void draw_pattern(const char* const pattern[], int width, int height, unsigned char color, int x, int y) {
    draw_logo_pattern(pattern, width, height, color, x, y);
}

static const char* m[] = {
    "M    M",
    "MM  MM",
    "M MM M",
    "M MM M",
    "M    M",
    "M    M"
};

static const char* y_pattern[] = {
    "      ",
    "y    y",
    "y    y",
    " yy y ",
    "  yy  ",
    " yy   "
};

static const char* d[] = {
    "DDDDD ",
    "D    D",
    "D    D",
    "D    D",
    "D    D",
    "DDDDD "
};

static const char* o[] = {
    " OOOO ",
    "O    O",
    "O    O",
    "O    O",
    "O    O",
    " OOOO "
};

static const char* s[] = {
    " SSSSS",
    "S     ",
    " SSSS ",
    "     S",
    "     S",
    "SSSSS "
};

void print_logo_color(int x, int start_y, unsigned char color) {
    draw_logo_pattern(m, 6, 6, color, x + 2, start_y);
    draw_logo_pattern(y_pattern, 6, 6, color, x + 9, start_y);
    draw_logo_pattern(d, 6, 6, color, x + 16, start_y);
    draw_logo_pattern(o, 6, 6, color, x + 23, start_y);
    draw_logo_pattern(s, 6, 6, color, x + 30, start_y);
}

void print_logo_at(int x, int start_y) {
    draw_logo_pattern(m, 6, 6, DARK_GREY, x + 2, start_y);
    draw_logo_pattern(y_pattern, 6, 6, DARK_GREY, x + 9, start_y);
    draw_logo_pattern(d, 6, 6, RED, x + 16, start_y);
    draw_logo_pattern(o, 6, 6, GREEN, x + 23, start_y);
    draw_logo_pattern(s, 6, 6, BLUE, x + 30, start_y);
}

void print_logo(void) {
    print_logo_at(0, 0);
}

void print(const char* msg, unsigned char color, int x, int y) {
    color = theme_attribute(color);

    int offset = y * active_screen_width + x;

    for (int i = 0; msg[i] != '\0'; i++) {
        if (offset >= active_screen_width * active_screen_height) break;

        unsigned char character = (unsigned char)msg[i];
        if (character == 0xC2 &&
            (unsigned char)msg[i + 1] == 0xA7) {
            character = 0x15;
            i++;
        } else if (character == 0xC3) {
            unsigned char continuation = (unsigned char)msg[i + 1];
            switch (continuation) {
                case 0x84: character = 0x8E; break;
                case 0x96: character = 0x99; break;
                case 0x9C: character = 0x9A; break;
                case 0x9F: character = 0xE1; break;
                case 0xA4: character = 0x84; break;
                case 0xB6: character = 0x94; break;
                case 0xBC: character = 0x81; break;
                default: character = '?'; break;
            }
            i++;
        }

        write_cell(offset,
                   character | ((unsigned short)color << 8));
        offset++;
    }
}

void clear_screen(void) {
    unsigned short blank = (unsigned short)' ' | ((unsigned short)theme_attribute(LIGHT_GREY) << 8);

    for (int index = 0; index < active_screen_width * active_screen_height; index++) {
        write_cell(index, blank);
    }
}

void scroll_screen(int* cursor_y) {
    if (*cursor_y < active_screen_height) return;

    scroll_screen_region(0, active_screen_height - 1, 1);

    *cursor_y = active_screen_height - 1;
}

void scroll_screen_region(int first_row, int last_row, int rows) {
    if (first_row < 0 || last_row >= active_screen_height || first_row > last_row ||
        rows <= 0 || rows > last_row - first_row + 1) {
        return;
    }

    for (int row = first_row; row <= last_row - rows; row++) {
        for (int column = 0; column < active_screen_width; column++) {
            int destination = row * active_screen_width + column;
            int source = (row + rows) * active_screen_width + column;
            write_cell(destination, screen_cells[source]);
        }
    }

    unsigned short blank =
        (unsigned short)' ' | ((unsigned short)theme_attribute(LIGHT_GREY) << 8);
    for (int row = last_row - rows + 1; row <= last_row; row++) {
        for (int column = 0; column < active_screen_width; column++) {
            write_cell(row * active_screen_width + column, blank);
        }
    }
}
