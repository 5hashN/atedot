#include "../../include/common.h"
#include "../../include/atedot.h"
#include "../../include/expr.h"

// bresenham line generation in pixel space
int plot_line(Canvas *surf, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for(;;) {
        canvas_pixel_set(surf, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    return 0;
}

int plot_expr(Canvas *surf, const char *line, uint32_t color,
              double xmin, double xmax, double ymin, double ymax) {

    double xrange = xmax - xmin;
    double yrange = ymax - ymin;
    if (fabs(xrange) < 1e-9) xrange = 1.0;
    if (fabs(yrange) < 1e-9) yrange = 1.0;

    // skip "plot " if present
    const char *expr_str = line;
    if (strncmp(expr_str, "plot ", 5) == 0) expr_str += 5;

    int err = 0;
    for (int px = 0; px < surf->px_w; ++px) {
        double x_world = xmin + (double)px / (surf->px_w - 1) * xrange;

        double y_world = expr_eval(expr_str, x_world, &err);

        if (err) return -1;

        int py = (int)((ymax - y_world) / yrange * (surf->px_h - 1));

        if (py >= 0 && py < surf->px_h) {
            canvas_pixel_set(surf, px, py, color);
        }
    }

    return 0;
}
