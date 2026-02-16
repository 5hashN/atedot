#include "../include/common.h"
#include "../include/expr.h"
#include <ctype.h>

// recursive parser
// expr   -> term { (+|-) term }
// term   -> factor { (*|/|^) factor }
// factor -> (expr) | number | -factor | func(expr) | x

static const char *pos;
static double current_x;
static int *err_flag;

static double parse_expr();

static void skip_spaces() {
    while (isspace((unsigned char)*pos)) pos++;
}

static double parse_factor() {
    skip_spaces();
    double v = 0.0;

    if (*pos == '(') {
        pos++;
        v = parse_expr();
        skip_spaces();
        if (*pos == ')') pos++;
        else *err_flag = 1;
    }
    else if (isdigit((unsigned char)*pos) || *pos == '.') {
        char *end;
        v = strtod(pos, &end);
        if (pos == end) *err_flag = 1;
        pos = end;
    }
    else if (*pos == 'x') {
        pos++;
        v = current_x;
    }
    else if (*pos == '-') {
        pos++;
        v = -parse_factor();
    }
    else if (isalpha((unsigned char)*pos)) {
        // func: sin, cos, tan, exp, log, abs, sqrt
        char func[5] = {0};
        int i = 0;
        while (isalpha((unsigned char)*pos) && i < 4) func[i++] = *pos++;

        skip_spaces();
        if (*pos == '(') {
            pos++;
            double arg = parse_expr();
            skip_spaces();
            if (*pos == ')') pos++;
            else *err_flag = 1;

            if (strcmp(func, "sin") == 0) v = sin(arg);
            else if (strcmp(func, "cos") == 0) v = cos(arg);
            else if (strcmp(func, "tan") == 0) v = tan(arg);
            else if (strcmp(func, "exp") == 0) v = exp(arg);
            else if (strcmp(func, "log") == 0) v = log(arg);
            else if (strcmp(func, "abs") == 0) v = fabs(arg);
            else if (strcmp(func, "sqrt") == 0) v = sqrt(arg);
            else *err_flag = 1; // unknown function
        } else {
            *err_flag = 1;
        }
    }
    else {
        *err_flag = 1; // unexpected char
    }

    // handle ^ immediately after factor
    skip_spaces();
    if (*pos == '^') {
        pos++;
        double exponent = parse_factor();
        v = pow(v, exponent);
    }

    return v;
}

static double parse_term() {
    double v = parse_factor();
    skip_spaces();
    while (*pos == '*' || *pos == '/') {
        char op = *pos++;
        double rhs = parse_factor();
        if (op == '*') v *= rhs;
        else if (rhs != 0.0) v /= rhs;
        else { v = NAN; *err_flag = 1; } // div by zero
    }
    return v;
}

static double parse_expr() {
    double v = parse_term();
    skip_spaces();
    while (*pos == '+' || *pos == '-') {
        char op = *pos++;
        double rhs = parse_term();
        if (op == '+') v += rhs;
        else v -= rhs;
    }
    return v;
}

double expr_eval(const char *expression, double x, int *error) {
    pos = expression;
    current_x = x;
    if (error) *error = 0;
    else { int dummy; err_flag = &dummy; } // safe fallback
    err_flag = error;

    return parse_expr();
}