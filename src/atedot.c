#include "../include/common.h"

// braille
// map (col, row) in a 2x4 cell to braille bit index
static inline int braille_bit(int col, int row) {
    static const int left [4] = {0, 1, 2, 6}; // dots 1, 2, 3, 7
    static const int right[4] = {3, 4, 5, 7}; // dots 4, 5, 6, 8
    return col ? right[row] : left[row];
}

static inline uint32_t braille_cp(uint8_t mask) {
    return 0x2800u + (uint32_t)mask;
}

// encode one unicode codepoint to UTF-8 into out[4], return bytes written
static int utf8_encode(uint32_t cp, char out[4]) {
    if (cp < 0x80) {
        out[0] = (char)cp; return 1;
    } else if (cp < 0x800) {
        out[0] = 0xC0 | (cp >> 6);
        out[1] = 0x80 | (cp & 0x3F);
        return 2;
    } else if (cp < 0x10000) {
        out[0] = 0xE0 | (cp >> 12);
        out[1] = 0x80 | ((cp >> 6) & 0x3F);
        out[2] = 0x80 | (cp & 0x3F);
        return 3;
    } else {
        out[0] = 0xF0 | (cp >> 18);
        out[1] = 0x80 | ((cp >> 12) & 0x3F);
        out[2] = 0x80 | ((cp >> 6) & 0x3F);
        out[3] = 0x80 | (cp & 0x3F);
        return 4;
    }
}

static void print_cell(uint8_t mask) {
    char buf[4]; int n = utf8_encode(braille_cp(mask), buf);
    fwrite(buf, 1, (size_t)n, stdout);
}

static void print_cell_with_color(uint8_t mask, uint32_t color) {
    char buf[4]; int n = utf8_encode(braille_cp(mask), buf);
    printf("\x1b[38;2;%d;%d;%dm", (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF); // set fg color
    fwrite(buf, 1, (size_t)n, stdout);
    printf("\x1b[0m"); // reset
}

// canvas
typedef struct {
    int px_w, px_h;         // pixel-space size
    int cell_w, cell_h;     // braille character grid
    uint8_t *cells;         // bits map to braille dots
    uint32_t *colors;       // color per pixel
} Canvas;

static uint32_t cell_color(const Canvas *surf, int x, int y) {
    uint8_t mask = surf->cells[y * surf->cell_w + x];
    if (mask == 0) return 0xFFFFFF;

    for (int dy = 0; dy < 4; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            int bit = braille_bit(dx, dy);
            if (mask & (1u << bit)) {
                int px = x*2 + dx, py = y*4 + dy;
                if (px < surf->px_w && py < surf->px_h) {
                    return surf->colors[py * surf->px_w + px];
                }
            }
        }
    }
    return 0xFFFFFF;
}

Canvas canvas_make(int px_w, int px_h) {
    Canvas surf;
    surf.px_w = px_w;
    surf.px_h = px_h;

    surf.cell_w = (px_w + 1) / 2;  // ceil(px_w / 2)
    surf.cell_h = (px_h + 3) / 4;  // ceil(px_h / 4)

    size_t n = (size_t)surf.cell_w * (size_t)surf.cell_h;

    surf.cells = (uint8_t*)calloc(n, 1);
    surf.colors = (uint32_t*)calloc((size_t)px_w * (size_t)px_h, sizeof(uint32_t));
    return surf;
}

void canvas_resize(Canvas *surf, int new_w, int new_h) {
    free(surf->cells);
    free(surf->colors);

    surf->px_w = new_w;
    surf->px_h = new_h;
    surf->cell_w = (new_w + 1) / 2;
    surf->cell_h = (new_h + 3) / 4;

    surf->cells = (uint8_t*)calloc((size_t)(surf->cell_w * surf->cell_h), sizeof(uint8_t));
    surf->colors = (uint32_t*)calloc((size_t)(surf->px_w * surf->px_h), sizeof(uint32_t));
}

void canvas_free(Canvas *surf) {
    free(surf->cells); surf->cells = NULL;
    free(surf->colors); surf->colors = NULL;
}

void canvas_clear(Canvas *surf) {
    memset(surf->cells, 0, (size_t)surf->cell_w * (size_t)surf->cell_h);
    memset(surf->colors, 0, (size_t)surf->px_w * (size_t)surf->px_h * sizeof(uint32_t));
}

// set a single pixel (x, y) in pixel space
void canvas_pixel_set(Canvas *surf, int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= surf->px_w || y >= surf->px_h) return;
    int cx = x / 2; int cy = y / 4;
    int col = x % 2; int row = y % 4;
    int bit = braille_bit(col, row);

    surf->cells[cy * surf->cell_w + cx] |= (1u << bit);
    surf->colors[y * surf->px_w + x] = color;
}

// unset pixel
void canvas_pixel_unset(Canvas *surf, int x, int y) {
    if (x < 0 || y < 0 || x >= surf->px_w || y >= surf->px_h) return;
    int cx = x / 2; int cy = y / 4;
    int col = x % 2; int  row = y % 4;
    int bit = braille_bit(col, row);

    surf->cells[cy * surf->cell_w + cx] &= ~(1u << bit);
    surf->colors[y * surf->px_w + x] = 0;
}

// to stdout
// dump to stdout with utf-8 braille and space for the rest
void render_row(const Canvas *surf, int y, bool use_color) {
    for (int x = 0; x < surf->cell_w; ++x) {
        uint8_t mask = surf->cells[y * surf->cell_w + x];
        if (!mask) { putchar(' '); continue; }

        if (use_color) {
            uint32_t color = cell_color(surf, x, y);
            print_cell_with_color(mask, color);
        } else print_cell(mask);
    }
    putchar('\n');
}

void render_full(const Canvas *surf, bool use_color) {
    for (int y = 0; y < surf->cell_h; ++y) {
        render_row(surf, y, use_color);
    }
}

void render_full_w_axes(const Canvas *surf,
                     double xmin, double xmax, double ymin, double ymax,
                     int x_ticks, int y_ticks, bool use_color) {

    int pad = 8;
    char buf[64];
    int y_step = surf->cell_h / (y_ticks - 1);

    for (int y = 0; y < surf->cell_h; ++y) {
        // print Y-axis labels
        if (y % y_step == 0 || y == surf->cell_h - 1) {
            double yval = ymax - (y / (double)(surf->cell_h - 1)) * (ymax - ymin);
            snprintf(buf, sizeof(buf), "%.2f", yval);
            printf("%*s ", pad-1, buf);
        } else {
            printf("%*s ", pad-1, "");
        }

        render_row(surf, y, use_color);
    }

    // print X-axis
    printf("%*s", pad, "");
    for (int i = 0; i < surf->cell_w; ++i) {
        if (x_ticks > 1 && i % (surf->cell_w / (x_ticks - 1)) == 0) {
            double xval = xmin + (i / (double)(surf->cell_w - 1)) * (xmax - xmin);
            snprintf(buf, sizeof(buf), "%.2f", xval);
            int len = (int)strlen(buf);
            printf("%s", buf);
            i += len - 1;
        } else putchar(' ');
    }
    putchar('\n');
}
