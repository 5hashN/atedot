#include "../include/common.h"
#include "../include/atedot.h"
#include "../include/expr.h"
#include "../include/repl.h"

int main() {
    setlocale(LC_ALL, "");

    Canvas surf = canvas_make(100, 64);    // 80x24 terminal pixels

    repl(&surf);    // interactive REPL

    canvas_free(&surf);
    return 0;
}
