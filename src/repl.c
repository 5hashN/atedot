#include <termios.h>
#include <unistd.h>
#include "../include/common.h"
#include "../include/atedot.h"

#define MAX_HISTORY 100
#define MAX_LINE 256
#define DEFAULT_COLOR 0x00FF00 // green
#define DEFAULT_CSV_COLOR 0x00FFFF // cyan

// state for zoom/pan
typedef struct {
    double xmin, xmax, ymin, ymax;
    bool locked; // true if user manually set view
} ViewState;

static ViewState view = { -10, 10, -5, 5, false };

static struct termios orig_termios;

static void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode() {
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
            if (len > 0 && *history_len < MAX_HISTORY) {
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
    int x_ticks = 5, y_ticks = 5;

    char *history[MAX_HISTORY] = {0};
    int history_len = 0, history_index = 0;

    enable_raw_mode();

    printf(" > ");
    fflush(stdout);

    while (readline(line, sizeof(line), history, &history_len, &history_index) > 0) {
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;

        if (strcmp(line, "clear") == 0) canvas_clear(surf);

        else if (strncmp(line, "reset", 5) == 0) {
            view.xmin = -10.0; view.xmax = 10.0;
            view.ymin = -5.0;  view.ymax = 5.0;
            view.locked = false;
            printf("Viewport reset.\n");
        }

        else if (strncmp(line, "zoom ", 5) == 0) {
            double f;
            if (sscanf(line+5, "%lf", &f) == 1 && f > 0) {
                double cx = (view.xmin + view.xmax)/2;
                double cy = (view.ymin + view.ymax)/2;
                double rx = (view.xmax - view.xmin)/2/f;
                double ry = (view.ymax - view.ymin)/2/f;
                view.xmin = cx - rx; view.xmax = cx + rx;
                view.ymin = cy - ry; view.ymax = cy + ry;
                view.locked = true;
                printf("Zoomed x%.2f\n", f);
            } else {
                printf("Usage: zoom <factor>\n");
            }
        }

        else if (strncmp(line, "size ", 5) == 0) {
            int w, h;
            if (sscanf(line + 5, "%d %d", &w, &h) == 2) {
                canvas_resize(surf, w, h);
                printf("Resized plot to %dx%d\n", w, h);
            } else printf("Usage: size <width> <height>\n");
        }

        else if (strncmp(line, "ticks", 5) == 0) {
            int xt, yt;
            if (sscanf(line + 5, "%d %d", &xt, &yt) == 2) {
                if (xt > 1) x_ticks = xt;
                if (yt > 1) y_ticks = yt;
                printf("Set ticks: x=%d, y=%d\n", x_ticks, y_ticks);
            } else printf("Usage: ticks <x_ticks> <y_ticks>\n");
        }

        else if (strncmp(line, "plot ", 5) == 0) {
            const char *p = line + 5;
            while (*p == ' ') p++;

            if (*p == '"' || *p == '\'') {
                char quote = *p++;
                const char *start = p;
                const char *end = strchr(start, quote);

                if (end) {
                    char filename[128];
                    size_t len = end - start;
                    if (len >= sizeof(filename)) len = sizeof(filename) - 1;

                    strncpy(filename, start, len);
                    filename[len] = '\0';

                    // parse columns after the closing quote
                    int xcol, ycol;
                    uint32_t color = DEFAULT_CSV_COLOR;
                    unsigned int hex_in;

                    int args = sscanf(end + 1, " %d %d %x", &xcol, &ycol, &hex_in);
                    if (args >= 2) {
                        if (args == 3) color = hex_in;
                            double x1, x2, y1, y2;

                            if (plot_from_csv(surf, filename, xcol, ycol, color, &x1, &x2, &y1, &y2) == 0) {
                                // snap view to data if not manually locked
                                if (!view.locked) {
                                    view.xmin = x1; view.xmax = x2;
                                    view.ymin = y1; view.ymax = y2;
                                }
                                printf("\n");
                                render_full_w_axes(surf, view.locked ? view.xmin : x1,
                                                        view.locked ? view.xmax : x2,
                                                        view.locked ? view.ymin : y1,
                                                        view.locked ? view.ymax : y2,
                                                        x_ticks, y_ticks, true);
                                printf("\n");
                        } else printf("Error: Could not read CSV file '%s'\n", filename);
                    } else printf("Usage: plot \"filename.csv\" <x_col> <y_col>\n");
                } else printf("Error: Unterminated string. Missing closing quote (%c).\n", quote);

            } else {
                // not a csv, assume math expression
                // detect trailing color
                char expr_buf[MAX_LINE];
                uint32_t color = DEFAULT_COLOR;

                strncpy(expr_buf, line, MAX_LINE);
                expr_buf[MAX_LINE-1] = '\0';

                char *last_space = strrchr(expr_buf, ' ');
                if (last_space) {
                    // check if string after space is a hex color
                    if (strncmp(last_space + 1, "0x", 2) == 0 || strncmp(last_space + 1, "0X", 2) == 0) {
                        if (parse_hex(last_space + 1, &color)) {
                            *last_space = '\0'; // truncate command so parser doesn't see color
                        }
                    }
                }

                if (plot_expr(surf, expr_buf, color, view.xmin, view.xmax, view.ymin, view.ymax) == 0) {
                    printf("\n");
                    render_full_w_axes(surf, view.xmin, view.xmax, view.ymin, view.ymax, x_ticks, y_ticks, true);
                    printf("\n");
                } else printf("Error: Invalid expression.\n");
            }

        } else printf("Error: Unknown command.\n");

        printf(" > ");
        fflush(stdout);
    }

    for(int i=0; i<history_len; i++) free(history[i]);
    disable_raw_mode();
}
