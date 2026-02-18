#include <termios.h>
#include <unistd.h>
#include "../include/common.h"
#include "../include/atedot.h"

#define MAX_CMD_HISTORY 100 // command line history
#define MAX_PLOT_HISTORY 50 // active plots on screen
#define MAX_LINE 256
#define DEFAULT_COLOR 0x00FF00 // green
#define DEFAULT_CSV_COLOR 0x00FFFF // cyan

// state for zoom/pan
typedef struct {
    double xmin, xmax, ymin, ymax;
    bool locked; // true if user manually set view
} ViewState;

typedef enum {
    PLOT_MODE_EXPR,
    PLOT_MODE_CSV
} PlotMode;

typedef struct {
    PlotMode mode;
    char source[MAX_LINE];
    uint32_t color;
    int col_x, col_y;
} PlotCmd;

static ViewState view = { -10, 10, -5, 5, false };

static struct termios orig_termios;

static PlotCmd plot_history[MAX_PLOT_HISTORY];
static int plot_count = 0;

static int global_x_ticks = 5;
static int global_y_ticks = 5;

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); // no canonical, no echo
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// helper to parse hex color from string 0xRRGGBB or RRGGBB
static bool parse_hex(const char *str, uint32_t *out) {
    char *end;
    unsigned long val = strtoul(str, &end, 16);
    if (end != str && *end == '\0') {
        *out = (uint32_t)val;
        return true;
    }
    return false;
}

// wipes canvas and redraws everything in history
static void replot_all(Canvas *surf) {
    canvas_clear(surf);

    for (int i = 0; i < plot_count; i++) {
        PlotCmd *cmd = &plot_history[i];

        if (cmd->mode == PLOT_MODE_EXPR) {
            plot_expr(surf, cmd->source, cmd->color,
                      view.xmin, view.xmax, view.ymin, view.ymax);
        }
        else if (cmd->mode == PLOT_MODE_CSV) {
            double d1, d2, d3, d4;
            plot_from_csv(surf, cmd->source, cmd->col_x, cmd->col_y, cmd->color,
                          &d1, &d2, &d3, &d4);
        }
    }
}

static void add_plot_expr(const char *expr, uint32_t color) {
    if (plot_count >= MAX_PLOT_HISTORY) return;
    plot_history[plot_count].mode = PLOT_MODE_EXPR;
    strncpy(plot_history[plot_count].source, expr, MAX_LINE-1);
    plot_history[plot_count].color = color;
    plot_count++;
}

static void add_plot_csv(const char *filename, int cx, int cy, uint32_t color) {
    if (plot_count >= MAX_PLOT_HISTORY) return;
    plot_history[plot_count].mode = PLOT_MODE_CSV;
    strncpy(plot_history[plot_count].source, filename, MAX_LINE-1);
    plot_history[plot_count].col_x = cx;
    plot_history[plot_count].col_y = cy;
    plot_history[plot_count].color = color;
    plot_count++;
}


// read one line with editing & history
static int readline(char *out, size_t out_size, char **history, int *history_len, int *history_index) {
    char buf[MAX_LINE] = {0};
    int len = 0, cursor = 0;

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) continue;

        if (c == '\n') {
            buf[len] = '\0';
            printf("\n");
            strncpy(out, buf, out_size);
            out[out_size-1] = '\0';
            if (len > 0 && *history_len < MAX_CMD_HISTORY) {
                history[*history_len] = strdup(buf);
                (*history_len)++;
            }
            *history_index = *history_len;
            return len;
        }
        else if (c == 127 || c == 8) { // backspace
            if (cursor > 0) {
                memmove(buf + cursor - 1, buf + cursor, len - cursor + 1);
                len--;
                cursor--;
            }
        }
        else if (c == 27) { // escape seq
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) == 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) == 0) continue;
            if (seq[0] == '[') {
                if (seq[1] == 'A') { // up
                    if (*history_index > 0) {
                        (*history_index)--;
                        memset(buf, 0, MAX_LINE);

                        if (history[*history_index]) {
                            strncpy(buf, history[*history_index], MAX_LINE);
                            len = strlen(buf);
                            cursor = len;
                        }
                    }
                } else if (seq[1] == 'B') { // down
                    if (*history_index < *history_len) {
                        (*history_index)++;
                        memset(buf, 0, MAX_LINE);

                        if (*history_index < *history_len && history[*history_index]) {
                            strncpy(buf, history[*history_index], MAX_LINE);
                            len = strlen(buf);
                            cursor = len;
                        }
                    } else {
                        len = cursor = 0;
                        buf[0] = '\0';
                    }
                } else if (seq[1] == 'C') { // right
                    if (cursor < len) cursor++;
                } else if (seq[1] == 'D') { // left
                    if (cursor > 0) cursor--;
                }
            }
        }
        else if (c >= 32 && c <= 126) { // printable
            if (len < MAX_LINE - 1) {
                memmove(buf + cursor + 1, buf + cursor, len - cursor + 1);
                buf[cursor] = c;
                len++;
                cursor++;
            }
        }

        printf("\r > ");                    // move to start + print prompt
        printf("%s\033[K", buf);            // print current buffer
        printf(" ");                        // overwrite leftover char if buffer shrank
        printf("\r\033[%dC", 3 + cursor);   // move cursor to correct spot
        fflush(stdout);
    }
}

void repl(Canvas *surf) {
    char line[MAX_LINE];
    char *cmd_history[MAX_CMD_HISTORY] = {0};
    int cmd_hist_len = 0, cmd_hist_idx = 0;

    enable_raw_mode();

    printf(" > ");
    fflush(stdout);

    while (readline(line, sizeof(line), cmd_history, &cmd_hist_len, &cmd_hist_idx) > 0) {
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;

        if (strcmp(line, "clear") == 0 || strcmp(line, "clean") == 0) {
            plot_count = 0; // reset history
            canvas_clear(surf);
            printf("Canvas cleared.\n");
        }

        else if (strncmp(line, "reset", 5) == 0) {
            view.xmin = -10.0; view.xmax = 10.0;
            view.ymin = -5.0;  view.ymax = 5.0;
            view.locked = false;

            replot_all(surf);
            printf("\n");
            render_full_w_axes(surf, view.xmin, view.xmax, view.ymin, view.ymax, global_x_ticks, global_y_ticks, true);
            printf("\nViewport reset.\n");
        }

        else if (strncmp(line, "zoom ", 5) == 0) {
            double fx, fy;
            int args = sscanf(line + 5, "%lf %lf", &fx, &fy);

            if (args == 1) fy = fx;
            if (args >= 1 && fx > 0 && fy > 0) {
                double cx = (view.xmin + view.xmax) / 2.0;
                double cy = (view.ymin + view.ymax) / 2.0;
                double rx = (view.xmax - view.xmin) / 2.0 / fx;
                double ry = (view.ymax - view.ymin) / 2.0 / fy;
                view.xmin = cx - rx; view.xmax = cx + rx;
                view.ymin = cy - ry; view.ymax = cy + ry;
                view.locked = true;

                replot_all(surf);

                printf("\n");
                render_full_w_axes(surf, view.xmin, view.xmax, view.ymin, view.ymax, global_x_ticks, global_y_ticks, true);

                if (args == 1) {
                    printf("\nZoomed x%.2f (Uniform)\n", fx);
                } else {
                    printf("\nZoomed X: x%.2f, Y: x%.2f\n", fx, fy);
                }
            } else {
                printf("Usage: zoom <factor>  OR  zoom <x_factor> <y_factor>\n");
            }
        }

        else if (strncmp(line, "size ", 5) == 0) {
            int w, h;
            if (sscanf(line + 5, "%d %d", &w, &h) == 2) {
                canvas_resize(surf, w, h);

                replot_all(surf);
                printf("\n");
                render_full_w_axes(surf, view.xmin, view.xmax, view.ymin, view.ymax, global_x_ticks, global_y_ticks, true);
                printf("\nResized to %dx%d\n", w, h);
            } else printf("Usage: size <width> <height>\n");
        }

        else if (strncmp(line, "ticks", 5) == 0) {
            int xt, yt;
            if (sscanf(line + 5, "%d %d", &xt, &yt) == 2) {
                if (xt > 1) global_x_ticks = xt;
                if (yt > 1) global_y_ticks = yt;

                printf("\n");
                render_full_w_axes(surf, view.xmin, view.xmax, view.ymin, view.ymax, global_x_ticks, global_y_ticks, true);
                printf("\nTicks set: x=%d, y=%d\n", global_x_ticks, global_y_ticks);
            } else printf("Usage: ticks <x_ticks> <y_ticks>\n");
        }

        else if (strncmp(line, "plot", 4) == 0) {
            char *p = line + 4;
            while (*p == ' ' || *p == '\t') p++;

            // "plot" -> view current state
            if (*p == '\0' || *p == '\n') {
                printf("\n");
                render_full_w_axes(surf, view.xmin, view.xmax, view.ymin, view.ymax, global_x_ticks, global_y_ticks, true);
                printf("\n");
            }
            // "plot ..." -> new plot
            else {
                if (*p == '"' || *p == '\'') {
                    char quote = *p++;
                    char *end = strchr(p, quote);
                    if (end) {
                        char filename[128];
                        size_t len = end - p;
                        if (len >= sizeof(filename)) len = sizeof(filename)-1;
                        strncpy(filename, p, len);
                        filename[len] = '\0';

                        int xcol, ycol;
                        uint32_t color = DEFAULT_CSV_COLOR;
                        unsigned int hex_in;
                        int args = sscanf(end + 1, " %d %d %x", &xcol, &ycol, &hex_in);

                        if (args >= 2) {
                            if (args == 3) color = hex_in;

                            add_plot_csv(filename, xcol, ycol, color);

                            double d1, d2, d3, d4;
                            if (!view.locked) {
                                if (plot_from_csv(surf, filename, xcol, ycol, color, &d1, &d2, &d3, &d4)) {
                                    view.xmin = d1; view.xmax = d2;
                                    view.ymin = d3; view.ymax = d4;
                                }
                            }

                            replot_all(surf);

                            printf("\n");
                            render_full_w_axes(surf, view.xmin, view.xmax, view.ymin, view.ymax, global_x_ticks, global_y_ticks, true);
                            printf("\n");
                        } else printf("Usage: plot \"file.csv\" <x_col> <y_col> [hex_color]\n");
                    } else printf("Error: Missing closing quote.\n");
                }
                else {
                    char expr_buf[MAX_LINE];
                    uint32_t color = DEFAULT_COLOR;
                    strncpy(expr_buf, line, MAX_LINE);

                    char *expr_start = expr_buf + 5;
                    while(*expr_start == ' ') expr_start++;

                    char *last_space = strrchr(expr_start, ' ');
                    if (last_space) {
                        if (strncmp(last_space + 1, "0x", 2) == 0 || strncmp(last_space + 1, "0X", 2) == 0) {
                            if (parse_hex(last_space + 1, &color)) {
                                *last_space = '\0';
                            }
                        }
                    }

                    add_plot_expr(expr_start, color);

                    replot_all(surf);

                    printf("\n");
                    render_full_w_axes(surf, view.xmin, view.xmax, view.ymin, view.ymax, global_x_ticks, global_y_ticks, true);
                    printf("\n");
                }
            }
        }
        else printf("Error: Unknown command.\n");

        printf(" > ");
        fflush(stdout);
    }

    for(int i=0; i<cmd_hist_len; i++) free(cmd_history[i]);
    disable_raw_mode();
}
