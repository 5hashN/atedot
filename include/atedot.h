#pragma once
#include "common.h"

typedef struct {
    int px_w, px_h;         // pixel-space size
    int cell_w, cell_h;     // braille character grid
    uint8_t *cells;         // bits map to braille dots
    uint32_t *colors;       // color per pixel
} Canvas;

Canvas canvas_make(int px_w, int px_h); // (col, row)
void canvas_resize(Canvas *surf, int new_w, int new_h);
void canvas_free(Canvas *surf);
void canvas_clear(Canvas *surf);

void canvas_pixel_set(Canvas *surf, int x, int y, uint32_t color); // single pixel (x, y)
void canvas_pixel_unset(Canvas *surf, int x, int y);

void render_row(const Canvas *surf, int y, bool use_color);
void render_full(const Canvas *surf, bool use_color);
void render_full_w_axes(const Canvas *surf,
                double xmin, double xmax, double ymin, double ymax,
                int x_ticks, int y_ticks, bool use_color);

int plot_line(Canvas *surf, int x0, int y0, int x1, int y1, uint32_t color); // bresenham line

int plot_expr(Canvas *surf, const char *func, uint32_t color,
              double xmin, double xmax, double ymin, double ymax);

int plot_from_csv(Canvas *surf, const char *path, int xcol, int ycol, uint32_t color,
                double *out_xmin, double *out_xmax, double *out_ymin, double *out_ymax);